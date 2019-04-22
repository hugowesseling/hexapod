import sys, os
import pygame
import array
import paho.mqtt.client as mqtt
sys.path.insert(1, os.path.join(sys.path[0], '../shared'))
import gamepad_helper

"""
def joystick2bytestring(joystick):
  #Parts to encode: axis 0-4, button 0-9, hat 0
  intArray = []
  #axes 0-4
  for axis in range(0,5):
    axisValue = joystick.get_axis(axis)
    axisByte = gamepad_helper.float01to255int(axisValue)
    intArray.append(axisByte)
  #button 0-9
  for button in range(0,10):
    buttonValue = joystick.get_button(button)
    buttonByte = 1 if buttonValue else 0
    intArray.append(buttonByte)
  byteString = array.array('B',intArray).tostring()
  if len(byteString) != gamepad_helper.packetSize:
    print("ERROR: Not encoding into correct packetSize: len(%r)=%d != %d"%(byteString,len(byteString),gamepad_helper.packetSize))
    exit(1)
  return byteString
"""
POWERBUTTON_ID = 0
prev_powerbutton_value = 1
poweron = False
def joystick2string(joystick):
    global prev_powerbutton_value, poweron
    powerbutton_value = joystick.get_button(POWERBUTTON_ID)
    if powerbutton_value and not prev_powerbutton_value:
        # Power switch
        poweron = not poweron
        print("Power on:", poweron)
        return "P "+("1" if poweron else "0")
    prev_powerbutton_value = powerbutton_value
    # get axis values for R command
    if poweron:
        axis0 = joystick.get_axis(0)
        axis1 = joystick.get_axis(1)
        axis2 = joystick.get_axis(3)
        return "W {:.2f} {:.2f} {:.2f} 20".format(axis0, axis1, axis2)
    else:
        return "X"

# Define some colors
BLACK    = (   0,   0,   0)
WHITE    = ( 255, 255, 255)

# This is a simple class that will help us print to the screen
# It has nothing to do with the joysticks, just outputing the
# information.
class TextPrint:
    def __init__(self):
        self.reset()
        self.font = pygame.font.Font(None, 20)

    def textprint(self, screen, textString):
        textBitmap = self.font.render(textString, True, BLACK)
        screen.blit(textBitmap, [self.x, self.y])
        self.y += self.line_height
        
    def reset(self):
        self.x = 10
        self.y = 10
        self.line_height = 15
        
    def indent(self):
        self.x += 10
        
    def unindent(self):
        self.x -= 10
    
client = mqtt.Client()
client.connect("hugowesseling.synology.me", 1883, 60)
client.loop_start()

pygame.init()

# Set the width and height of the screen [width,height]
size = [500, 700]
screen = pygame.display.set_mode(size)

pygame.display.set_caption("My Game")

#Loop until the user clicks the close button.
done = False

# Used to manage how fast the screen updates
clock = pygame.time.Clock()

# Initialize the joysticks
pygame.joystick.init()
    
# Get ready to print
textPrint = TextPrint()

# -------- Main Program Loop -----------
while done==False:
    # EVENT PROCESSING STEP
    for event in pygame.event.get(): # User did something
        if event.type == pygame.QUIT: # If user clicked close
            done=True # Flag that we are done so we exit this loop
        
        # Possible joystick actions: JOYAXISMOTION JOYBALLMOTION JOYBUTTONDOWN JOYBUTTONUP JOYHATMOTION
        if event.type == pygame.JOYBUTTONDOWN:
            print("Joystick button pressed.")
        if event.type == pygame.JOYBUTTONUP:
            print("Joystick button released.")
            
 
    # DRAWING STEP
    # First, clear the screen to white. Don't put other drawing commands
    # above this, or they will be erased with this command.
    screen.fill(WHITE)
    textPrint.reset()

    # Get count of joysticks
    joystick_count = pygame.joystick.get_count()

    textPrint.textprint(screen, "Number of joysticks: {}".format(joystick_count) )
    textPrint.indent()
    
    # For each joystick:
    for i in range(joystick_count):
        joystick = pygame.joystick.Joystick(i)
        joystick.init()
    
        textPrint.textprint(screen, "Joystick {}".format(i) )
        textPrint.indent()
    
        # Get the name from the OS for the controller/joystick
        name = joystick.get_name()
        textPrint.textprint(screen, "Joystick name: {}".format(name) )
        
        # Usually axis run in pairs, up/down for one, and left/right for
        # the other.
        axes = joystick.get_numaxes()
        textPrint.textprint(screen, "Number of axes: {}".format(axes) )
        textPrint.indent()
        
        for i in range( axes ):
            axis = joystick.get_axis( i )
            textPrint.textprint(screen, "Axis {} value: {:>6.3f}".format(i, axis) )
        textPrint.unindent()
            
        buttons = joystick.get_numbuttons()
        textPrint.textprint(screen, "Number of buttons: {}".format(buttons) )
        textPrint.indent()

        for i in range( buttons ):
            button = joystick.get_button( i )
            textPrint.textprint(screen, "Button {:>2} value: {}".format(i,button) )
        textPrint.unindent()
            
        # Hat switch. All or nothing for direction, not like joysticks.
        # Value comes back in an array.
        hats = joystick.get_numhats()
        textPrint.textprint(screen, "Number of hats: {}".format(hats) )
        textPrint.indent()

        for i in range( hats ):
            hat = joystick.get_hat( i )
            textPrint.textprint(screen, "Hat {} value: {}".format(i, str(hat)) )
        textPrint.unindent()
        
        textPrint.unindent()

    
    # ALL CODE TO DRAW SHOULD GO ABOVE THIS COMMENT
    
    # Go ahead and update the screen with what we've drawn.
    pygame.display.flip()
    send_string = joystick2string(joystick = pygame.joystick.Joystick(0))
    print("send_string: %r"%send_string)
    client.publish("motioncontrol", send_string)

    # Limit to 20 frames per second
    clock.tick(10)
    
# Close the window and quit.
# If you forget this line, the program will 'hang'
# on exit if running from IDLE.
pygame.quit ()
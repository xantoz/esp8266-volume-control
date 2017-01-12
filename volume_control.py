import math
from mcp42xxx import MCP42XXX

# A logarithmic mapping from 0 to 100 (volume levels) to 0 to 255
# (values we send to the potentiometers). We need this because the
# MCP42010 we use is not a logarithmic potentiometer, and volume
# potentiometers usually are logarithmic due to how sound is
# perceived. Unfortunately this mapping gives us a bit less precision
# than the potentiometers themselves allow for, and some of the upper
# positions are bound to be duplicated (a quick calculation gives 89
# unique volume levels), but doing this is the next best thing to be
# able to sanely adjust volume level without getting a proper
# logarithmic digital potentiometer.
logarithmic_mapping = [0] + [int(MCP42XXX.MAX_VALUE*(math.log(i)/math.log(100)) + 1.0) for i in range(1, 100)]

# Our 6 channel volume controller
class VolumeController:
    def __init__(self):
        self.pot = MCP42XXX(daisyCount=3)
        self.levels = {
            'FL': 0,
            'RR': 0,
            'RL': 0,
            'RR': 0,
            'CEN': 0,
            'SUB': 0
        }
        
    def volume_sweep(self):
        """Test routine that sweeps the volume of all potentiometers"""
        while True:
            for i in logarithmic_mapping:
                self.pot.set_chain([i, i, i])
            for i in reversed(logarithmic_mapping):
                self.pot.set_chain([i, i, i])
        

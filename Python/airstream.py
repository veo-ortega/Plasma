""" *==LICENSE==*

CyanWorlds.com Engine - MMOG client, server and tools
Copyright (C) 2011  Cyan Worlds, Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

You can contact Cyan Worlds, Inc. by email legal@cyan.com
 or by snail mail at:
      Cyan Worlds, Inc.
      14617 N Newport Hwy
      Mead, WA   99021

 *==LICENSE==* """


from Plasma import *
from PlasmaTypes import *
import PlasmaControlKeys

activateAirstream = ptAttribActivator(1,"airstream activator")
airstreamNode = ptAttribSceneobject(2,"airstream attach node")

class airstream(ptResponder):

    def __init__(self):
        # run parent class init
        ptResponder.__init__(self)
        self.id = 50439
        self.version = 1
        
        print "__init__airstream v."

                        
    def OnNotify(self,state,id,events):

        if id==activateAirstream.id:
            avatar = PtFindAvatar(events)
            avatar.draw.disable()
#            avatar.physics.warpObj(airstreamNode.value.getKey())
#            PtAttachObject(avatar.getKey(), airstreamNode.value.getKey())
            avatar.physics.enable(0)
 

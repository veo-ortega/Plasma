/*==LICENSE==*

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

Additional permissions under GNU GPL version 3 section 7

If you modify this Program, or any covered work, by linking or
combining it with any of RAD Game Tools Bink SDK, Autodesk 3ds Max SDK,
NVIDIA PhysX SDK, Microsoft DirectX SDK, OpenSSL library, Independent
JPEG Group JPEG library, Microsoft Windows Media SDK, or Apple QuickTime SDK
(or a modified version of those libraries),
containing parts covered by the terms of the Bink SDK EULA, 3ds Max EULA,
PhysX SDK EULA, DirectX SDK EULA, OpenSSL and SSLeay licenses, IJG
JPEG Library README, Windows Media SDK EULA, or QuickTime SDK EULA, the
licensors of this Program grant you additional
permission to convey the resulting work. Corresponding Source for a
non-source form of such a combination shall include the source code for
the parts of OpenSSL and IJG JPEG Library used as well as that of the covered
work.

You can contact Cyan Worlds, Inc. by email legal@cyan.com
 or by snail mail at:
      Cyan Worlds, Inc.
      14617 N Newport Hwy
      Mead, WA   99021

*==LICENSE==*/
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//  pfGameGUIMgr                                                            //
//                                                                          //
//// History /////////////////////////////////////////////////////////////////
//                                                                          //
//  11.13.2001 mcn  - Created                                               //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

#include "pfGameGUIMgr.h"

#include "HeadSpin.h"
#include "hsResMgr.h"
#include "hsTimer.h"

#include "pfGUIControlMod.h"
#include "pfGUIDialogMod.h"
#include "pfGUIDialogHandlers.h"
#include "pfGUIDialogNotifyProc.h"
#include "pfGUIPopUpMenu.h"
#include "pfGUITagDefs.h"

#include "pnInputCore/plKeyMap.h"
#include "pnKeyedObject/plFixedKey.h"
#include "pnMessage/plClientMsg.h"
#include "pnSceneObject/plSceneObject.h"    // So we can get the target sceneNode of a dialog

#include "plInputCore/plInputDevice.h"
#include "plInputCore/plInputInterface.h"
#include "plInputCore/plInputInterfaceMgr.h"
#include "plMessage/plConsoleMsg.h"
#include "plMessage/plInputEventMsg.h"
#include "plMessage/plInputIfaceMgrMsg.h"
#include "plResMgr/plKeyFinder.h"

#include "pfMessage/pfGameGUIMsg.h"


//////////////////////////////////////////////////////////////////////////////
//// pfGameUIInputInterface Definition ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class pfGameUIInputInterface : public plInputInterface
{
    protected:
        pfGameGUIMgr    * const fGUIManager;

        uint8_t   fModifiers;
        uint8_t   fButtonState;
        bool    fHaveInterestingCursor;
        uint32_t  fCurrentCursor;

        bool    IHandleCtrlCmd(plCtrlCmd *cmd) override;
        bool    IControlCodeEnabled(ControlEventCode code) override;

    public:

        pfGameUIInputInterface( pfGameGUIMgr * const mgr );

        uint32_t  GetPriorityLevel() const override { return kGUISystemPriority; }
        bool    InterpretInputEvent(plInputEventMsg *pMsg) override;
        uint32_t  GetCurrentCursorID() const override;
        float GetCurrentCursorOpacity() const override;
        bool    HasInterestingCursorID() const override { return fHaveInterestingCursor; }
        virtual bool    SwitchInterpretOrder() const { return true; }

        void    RestoreDefaultKeyMappings() override
        {
            if( fControlMap != nil )
            {
                fControlMap->UnmapAllBindings();
                fControlMap->BindKey( KEY_BACKSPACE, B_CONTROL_EXIT_GUI_MODE, plKeyMap::kFirstAlways );
                fControlMap->BindKey( KEY_ESCAPE, B_CONTROL_EXIT_GUI_MODE, plKeyMap::kSecondAlways );
            }
        }
};

//////////////////////////////////////////////////////////////////////////////
//// pfGameGUIMgr Functions //////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

pfGameGUIMgr    *pfGameGUIMgr::fInstance = nil;


//// Constructor & Destructor ////////////////////////////////////////////////

pfGameGUIMgr::pfGameGUIMgr()
    : fActivated(), fInputCtlIndex(), fActiveDialogs(), fInputConfig(),
      fDefaultCursor(plInputInterface::kCursorUp), fCursorOpacity(1.f), fAspectRatio(),
      fActiveDlgCount()
{
    fInstance = this;
}

pfGameGUIMgr::~pfGameGUIMgr()
{
    // the GUIMgr is dead!
    fInstance = nil;

    for (pfGUIDialogMod* dialog : fDialogs)
        UnloadDialog(dialog);

    for (pfDialogNameSetKey* dlgKey : fDialogToSetKeyOf)
        delete dlgKey;

    if( fActivated )
        IActivateGUI( false );

    delete fInputConfig;
}


//// Init ////////////////////////////////////////////////////////////////////

bool    pfGameGUIMgr::Init()
{
    return true;
}

//// Draw ////////////////////////////////////////////////////////////////////

void    pfGameGUIMgr::Draw( plPipeline *p )
{
}

//// MsgReceive //////////////////////////////////////////////////////////////

bool    pfGameGUIMgr::MsgReceive( plMessage* pMsg )
{
    pfGameGUIMsg    *guiMsg = pfGameGUIMsg::ConvertNoRef( pMsg );
    if( guiMsg != nil )
    {
        if( guiMsg->GetCommand() == pfGameGUIMsg::kLoadDialog )
            LoadDialog(guiMsg->GetString().c_str(), nil, guiMsg->GetAge().c_str());
        else if( guiMsg->GetCommand() == pfGameGUIMsg::kShowDialog )
            IShowDialog(guiMsg->GetString().c_str());
        else if( guiMsg->GetCommand() == pfGameGUIMsg::kHideDialog )
            IHideDialog(guiMsg->GetString().c_str());

        return true;
    }

    plGenRefMsg *refMsg = plGenRefMsg::ConvertNoRef( pMsg );
    if( refMsg != nil )
    {
        if( refMsg->fType == kDlgModRef )
        {
            if( refMsg->GetContext() & ( plRefMsg::kOnCreate | plRefMsg::kOnRequest ) )
            {
                IAddDlgToList( refMsg->GetRef() );
            }
            else if( refMsg->GetContext() & plRefMsg::kOnReplace )
            {
                IRemoveDlgFromList( refMsg->GetOldRef() );
                IAddDlgToList( refMsg->GetRef() );
            }
            else if( refMsg->GetContext() & ( plRefMsg::kOnRemove | plRefMsg::kOnDestroy ) )
            {
                IRemoveDlgFromList( refMsg->GetRef() );
            }
        }
        return true;
    }

    return hsKeyedObject::MsgReceive( pMsg );
}

//// IAddDlgToList ///////////////////////////////////////////////////////////

void    pfGameGUIMgr::IAddDlgToList( hsKeyedObject *obj )
{
    if (std::find(fDialogs.cbegin(), fDialogs.cend(), (pfGUIDialogMod *)obj) == fDialogs.cend())
    {
        pfGUIDialogMod  *mod = pfGUIDialogMod::ConvertNoRef( obj );
        if( mod != nil )
        {
            mod->UpdateAspectRatio();   // adding a new dialog, make sure the correct aspect ratio is set
            fDialogs.emplace_back(mod);

            // check to see if it is the dialog we are waiting for to be loaded
            for (auto iter = fDialogToSetKeyOf.cbegin(); iter != fDialogToSetKeyOf.cend(); ++iter)
            {
                if (strcmp((*iter)->GetName(), mod->GetName()) == 0)
                {
                    SetDialogToNotify(mod, (*iter)->GetKey());
                    // now remove this entry... we did it
                    delete *iter;
                    fDialogToSetKeyOf.erase(iter);
                    // that's all the damage we can do for now...
                    break;
                }
            }
        }
    }

/*  // It's now officially "loaded"; take it off the pending list
    for( i = 0; i < fDlgsPendingLoad.GetCount(); i++ )
    {
        if( stricmp( fDlgsPendingLoad[ i ]->GetName(), ( (pfGUIDialogMod *)obj )->GetName() ) == 0 )
        {
            // Here it is
            delete fDlgsPendingLoad[ i ];
            fDlgsPendingLoad.Remove( i );
            break;
        }
    }
*/
}

//// IRemoveDlgFromList //////////////////////////////////////////////////////

void    pfGameGUIMgr::IRemoveDlgFromList( hsKeyedObject *obj )
{
    auto iter = std::find(fDialogs.cbegin(), fDialogs.cend(), (pfGUIDialogMod *)obj);
    if (iter != fDialogs.cend())
    {
        pfGUIDialogMod  *mod = pfGUIDialogMod::ConvertNoRef( obj );
        hsAssert( mod != nil, "Non-dialog sent to gameGUIMgr::IRemoveDlg()" );

        if( mod != nil )
        {
            if( mod->IsEnabled() )
            {
                mod->SetEnabled( false );

                mod->Unlink();
                if( fActiveDialogs == nil )
                    IActivateGUI( false );
            }

            // Needed?
//              GetKey()->Release( mod->GetKey() );
            fDialogs.erase(iter);
        }
    }

    // It's now officially "unloaded"; take it off the pending list
/*  int i;
    for( i = 0; i < fDlgsPendingUnload.GetCount(); i++ )
    {
        if( stricmp( fDlgsPendingUnload[ i ]->GetName(), ( (pfGUIDialogMod *)obj )->GetName() ) == 0 )
        {
            // Here it is
            delete fDlgsPendingUnload[ i ];
            fDlgsPendingUnload.Remove( i );
            break;
        }
    }
*/
}

//// LoadDialog //////////////////////////////////////////////////////////////

void    pfGameGUIMgr::LoadDialog( const char *name, plKey recvrKey, const char *ageName )
{
    // see if they want to set the receiver key once the dialog is loaded
    if ( recvrKey != nil )
    {
        // first see if we are loading a dialog that is already being loaded
        bool alreadyLoaded = std::any_of(fDialogToSetKeyOf.cbegin(), fDialogToSetKeyOf.cend(),
                                         [name](pfDialogNameSetKey* dlgKey) {
                                             return strcmp(dlgKey->GetName(), name) == 0;
                                         });
        if (!alreadyLoaded)
        {
            pfDialogNameSetKey* pDNSK = new pfDialogNameSetKey(name,recvrKey);
            fDialogToSetKeyOf.emplace_back(pDNSK);
        }
    }

    hsStatusMessageF("\nLoading Dialog %s %s ... %f\n", name, ( ageName != nil ) ? ageName : "GUI", hsTimer::GetSeconds() );

    plKey clientKey = hsgResMgr::ResMgr()->FindKey( kClient_KEY );

    plClientMsg *msg = new plClientMsg( plClientMsg::kLoadRoomHold );
    msg->AddReceiver( clientKey );
    msg->AddRoomLoc(plKeyFinder::Instance().FindLocation(ageName ? ageName : "GUI", name));
    msg->Send();

    // Now add this dialog to a list of pending loads (will remove it once it's fully loaded)
//  fDlgsPendingLoad.Append( new pfDialogNameSetKey( name, nil ) );
}

//// IShowDialog /////////////////////////////////////////////////////////////

void    pfGameGUIMgr::IShowDialog( const char *name )
{
    for (pfGUIDialogMod* dialog : fDialogs)
    {
        if (stricmp(dialog->GetName(), name) == 0)
        {
            ShowDialog(dialog);
            dialog->RefreshAllControls();
            break;
        }
    }
}

//// IHideDialog /////////////////////////////////////////////////////////////

void    pfGameGUIMgr::IHideDialog( const char *name )
{
    for (pfGUIDialogMod* dialog : fDialogs)
    {
        if (stricmp(dialog->GetName(), name) == 0)
        {
            HideDialog(dialog);
            break;
        }
    }
}

//// ShowDialog //////////////////////////////////////////////////////////////

void    pfGameGUIMgr::ShowDialog( pfGUIDialogMod *dlg, bool resetClickables /* = true */ )
{
    if ( resetClickables )
        plInputInterfaceMgr::GetInstance()->ResetClickableState();
    if( !dlg->IsEnabled() )
    {
        dlg->SetEnabled( true );

        // Add to active list
        if( fActiveDialogs == nil )
            IActivateGUI( true );
        
        dlg->LinkToList( &fActiveDialogs );

        return;
    }
}

//// HideDialog //////////////////////////////////////////////////////////////

void    pfGameGUIMgr::HideDialog( pfGUIDialogMod *dlg )
{
    if( dlg->IsEnabled() )
    {
        dlg->SetEnabled( false );

        dlg->Unlink();
        if( fActiveDialogs == nil )
            IActivateGUI( false );
    }
}

//// UnloadDialog ////////////////////////////////////////////////////////////
//  Destroy the dialog and all the things associated with it. Fun fun.

void    pfGameGUIMgr::UnloadDialog( const char *name )
{
    for (pfGUIDialogMod* dialog : fDialogs)
    {
        if (stricmp(dialog->GetName(), name) == 0)
        {
            UnloadDialog(dialog);
            break;
        }
    }
}

void    pfGameGUIMgr::UnloadDialog( pfGUIDialogMod *dlg )
{
//  IRemoveDlgFromList( dlg );

    // Add the name to our list of dialogs pending unload
//  fDlgsPendingUnload.Append( new pfDialogNameSetKey( dlg->GetName(), nil ) );

    plKey       sceneNodeKey = dlg->GetSceneNodeKey();
    if( sceneNodeKey == nil )
    {
        hsStatusMessageF( "Warning: Unable to grab sceneNodeKey to unload dialog %s; searching for it...", dlg->GetName() );
        sceneNodeKey = plKeyFinder::Instance().FindSceneNodeKey( dlg->GetKey()->GetUoid().GetLocation() );
    }

//  if( dlg->GetTarget() )
    if( sceneNodeKey != nil )
    {
        plKey clientKey = hsgResMgr::ResMgr()->FindKey( kClient_KEY );

        plClientMsg *msg = new plClientMsg( plClientMsg::kUnloadRoom );
        msg->AddReceiver( clientKey );
//      msg->SetProgressBarSuppression( true );
        msg->AddRoomLoc(sceneNodeKey->GetUoid().GetLocation());
        msg->Send();
    }
//  GetKey()->Release( dlg->GetKey() );
}

//// IsDialogLoaded ////// see if the dialog is in our list of loaded dialogs

bool    pfGameGUIMgr::IsDialogLoaded( const char *name )
{
    // search through all the dialogs we've loaded
    return std::any_of(fDialogs.cbegin(), fDialogs.cend(),
                       [name](pfGUIDialogMod* dialog) {
                           return strcmp(dialog->GetName(), name) == 0;
                       });
}

pfGUIPopUpMenu  *pfGameGUIMgr::FindPopUpMenu( const char *name )
{
    for (pfGUIDialogMod* dialog : fDialogs)
    {
        pfGUIPopUpMenu *menu = pfGUIPopUpMenu::ConvertNoRef(dialog);
        if( menu != nil && stricmp( menu->GetName(), name ) == 0 )
            return menu;
    }

    return nil;
}

std::vector<plPostEffectMod*> pfGameGUIMgr::GetDlgRenderMods() const
{
    std::vector<plPostEffectMod*> retVal;
    pfGUIDialogMod* curDialog = fActiveDialogs;
    while (curDialog)
    {
        retVal.push_back(curDialog->GetRenderMod());
        curDialog = curDialog->GetNext();
    }
    return retVal;
}

///// SetDialogToNotify     - based on name
// This will Set the handler to a Notify Handler that will send a GUINotifyMsg to the receiver
//
void pfGameGUIMgr::SetDialogToNotify(const char *name, plKey recvrKey)
{
    for (pfGUIDialogMod* dialog : fDialogs)
    {
        if (stricmp(dialog->GetName(), name) == 0)
        {
            SetDialogToNotify(dialog, recvrKey);
            break;
        }
    }
}

///// SetDialogToNotify     - pfGUIDialogMod*
// This will Set the handler to a Notify Handler that will send a GUINotifyMsg to the receiver
//
void pfGameGUIMgr::SetDialogToNotify(pfGUIDialogMod *dlg, plKey recvrKey)
{
    pfGUIDialogNotifyProc* handler = new pfGUIDialogNotifyProc(recvrKey);
    dlg->SetHandler(handler);
    handler->OnInit();
}


//// IActivateGUI ////////////////////////////////////////////////////////////
//  "Activates" the GUI manager. This means enabling the drawing of GUI
//  elements on the screen as well as rerouting input to us.

void    pfGameGUIMgr::IActivateGUI( bool activate )
{
    if( fActivated == activate )
        return;

    if( activate )
    {
        fInputConfig = new pfGameUIInputInterface( this );
        plInputIfaceMgrMsg *msg = new plInputIfaceMgrMsg( plInputIfaceMgrMsg::kAddInterface );
        msg->SetIFace( fInputConfig );
        msg->Send();
    }
    else
    {
        plInputIfaceMgrMsg *msg = new plInputIfaceMgrMsg( plInputIfaceMgrMsg::kRemoveInterface );
        msg->SetIFace( fInputConfig );
        msg->Send();

        hsRefCnt_SafeUnRef( fInputConfig );
        fInputConfig = nil;
    }

    fActivated = activate;
}

//// IHandleMouse ////////////////////////////////////////////////////////////
//  Distributes mouse events to the dialogs currently active

bool    pfGameGUIMgr::IHandleMouse( EventType event, float mouseX, float mouseY, uint8_t modifiers, uint32_t *desiredCursor ) 
{
    pfGUIDialogMod  *dlg;


    // Update interesting things first, no matter what, for ALL dialogs
    bool    modalPresent = false;
    for( dlg = fActiveDialogs; dlg != nil; dlg = dlg->GetNext() )
    {
        dlg->UpdateInterestingThings( mouseX, mouseY, modifiers, modalPresent );
        if (dlg->HasFlag( pfGUIDialogMod::kModal ))
            modalPresent = true;
    }

    for( dlg = fActiveDialogs; dlg != nil; dlg = dlg->GetNext() )
    {
        if( dlg->HandleMouseEvent( event, mouseX, mouseY, modifiers ) ||
            ( dlg->HasFlag( pfGUIDialogMod::kModal ) && event != pfGameGUIMgr::kMouseUp ) )
        {
            // If this dialog handled it, also get the cursor it wants
            *desiredCursor = dlg->GetDesiredCursor();
            return true;
        }
    }

    return false;
}

//// IHandleKeyEvt ///////////////////////////////////////////////////////////
//  Distributes mouse events to the dialogs currently active

bool    pfGameGUIMgr::IHandleKeyEvt( EventType event, plKeyDef key, uint8_t modifiers ) 
{
    pfGUIDialogMod  *dlg;


    for( dlg = fActiveDialogs; dlg != nil; dlg = dlg->GetNext() )
    {
        if( dlg->HandleKeyEvent( event, key, modifiers ) )
            return true;
    }

    return false;
}

//// IHandleKeyPress /////////////////////////////////////////////////////////
//  Like IHandleKeyPress, but takes in a char for distributing actual 
//  characters typed.

bool    pfGameGUIMgr::IHandleKeyPress( wchar_t key, uint8_t modifiers ) 
{
    pfGUIDialogMod  *dlg;

    // Really... Don't handle any nil keypresses
    if (!key)
        return false;

    for( dlg = fActiveDialogs; dlg != nil; dlg = dlg->GetNext() )
    {
        if( dlg->HandleKeyPress( key, modifiers ) )
            return true;
    }

    return false;
}

//// IModalBlocking //////////////////////////////////////////////////////////
//  Looks at the chain of active dialogs and determines if there's any modals
//  blocking input. Returns true if so.

bool    pfGameGUIMgr::IModalBlocking()
{
    return ( IGetTopModal() != nil ) ? true : false;
}

//// IGetTopModal ////////////////////////////////////////////////////////////
//  Returns the topmost (visible) modal dialog, nil if none.

pfGUIDialogMod  *pfGameGUIMgr::IGetTopModal() const
{
    pfGUIDialogMod  *dlg;


    for( dlg = fActiveDialogs; dlg != nil; dlg = dlg->GetNext() )
    {
        if( dlg->HasFlag( pfGUIDialogMod::kModal ) )
            return dlg;
    }

    return nil;
}


//////////////////////////////////////////////////////////////////////////////
//// Control Config Class ////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

pfGameUIInputInterface::pfGameUIInputInterface( pfGameGUIMgr * const mgr ) : plInputInterface(), fGUIManager( mgr )
{
    fModifiers = pfGameGUIMgr::kNoModifiers;
    fButtonState = 0;
    fHaveInterestingCursor = false;
    SetEnabled( true );         // Always enabled
    fCurrentCursor = kCursorUp;

    // Add our control codes to our control map. Do NOT add the key bindings yet.
    // Note: HERE is where you specify the actions for each command, i.e. net propagate and so forth.
    // This part basically declares us master of the bindings for these commands.
    
    // IF YOU ARE LOOKING TO CHANGE THE DEFAULT KEY BINDINGS, DO NOT LOOK HERE. GO TO
    // RestoreDefaultKeyMappings()!!!!

    fControlMap->AddCode( B_CONTROL_EXIT_GUI_MODE, kControlFlagNormal | kControlFlagNoRepeat );

    // IF YOU ARE LOOKING TO CHANGE THE DEFAULT KEY BINDINGS, DO NOT LOOK HERE. GO TO
    // RestoreDefaultKeyMappings()!!!!
}

bool    pfGameUIInputInterface::IControlCodeEnabled( ControlEventCode code )
{
    if( code == B_CONTROL_EXIT_GUI_MODE )
    {
        // Disable the exitGUIMode key binding if we don't have a modal dialog up or if 
        // the cursor is inside an edit or multiline edit control
        if( !fGUIManager->IModalBlocking() )
            return false;

        pfGUIDialogMod *dlg = fGUIManager->IGetTopModal();
        if( dlg != nil )
        {
            pfGUIControlMod *ctrl = dlg->GetFocus();
            if( ctrl != nil && ctrl->HasFlag( pfGUIControlMod::kTakesSpecialKeys ) )
                return false;
        }
    }
    return true;    // Enable all other codes
}

bool    pfGameUIInputInterface::IHandleCtrlCmd( plCtrlCmd *cmd )
{
    if( cmd->fControlCode == B_CONTROL_EXIT_GUI_MODE )
    {
        if( cmd->fControlActivated )
        {
            pfGUIDialogMod *dlg = fGUIManager->IGetTopModal();
            if( dlg != nil && dlg->GetHandler() != nil )
                dlg->GetHandler()->OnControlEvent( pfGUIDialogProc::kExitMode );
        }

        return true;
    }
    return false;
}

bool    pfGameUIInputInterface::InterpretInputEvent( plInputEventMsg *pMsg )
{
    bool        handled = false;


    /// The in-game UI has to do far more complicated control handling, so we just overload this entirely
    plKeyEventMsg *pKeyMsg = plKeyEventMsg::ConvertNoRef( pMsg );
    if( pKeyMsg )
    {
        // By default, we don't want the modifier keys treated as "handled", 'cause
        // we want the other interfaces to get them as well (unless we have a modal
        // as the top dialog).
        if( pKeyMsg->GetKeyCode() == KEY_SHIFT )
        {
            if( pKeyMsg->GetKeyDown() )
                fModifiers |= pfGameGUIMgr::kShiftDown;
            else
                fModifiers &= ~pfGameGUIMgr::kShiftDown;
        }
        else if( pKeyMsg->GetKeyCode() == KEY_CTRL )
        {
            if( pKeyMsg->GetKeyDown() )
                fModifiers |= pfGameGUIMgr::kCtrlDown;
            else
                fModifiers &= ~pfGameGUIMgr::kCtrlDown;
        }
        else if( pKeyMsg->GetKeyCode() == KEY_CAPSLOCK )
        {
            if( pKeyMsg->GetKeyDown() )
                fModifiers |= pfGameGUIMgr::kCapsDown;
            else
                fModifiers &= ~pfGameGUIMgr::kCapsDown;
        }
        else
        {
            // Sometimes I can't explain why Mathew does some of the things he does.
            // I going to replace his modifier flags (which I don't know why he thought he had to have his own)
            //   with the ones that are in the keymsg since they seem to be more accurate!
            fModifiers = 0;
            if ( pKeyMsg->GetShiftKeyDown() )
                fModifiers |= pfGameGUIMgr::kShiftDown;
            if ( pKeyMsg->GetCtrlKeyDown() )
                fModifiers |= pfGameGUIMgr::kCtrlDown;
            if ( pKeyMsg->GetCapsLockKeyDown() )
                fModifiers |= pfGameGUIMgr::kCapsDown;
            if( pKeyMsg->GetKeyDown() )
            {
                if( !pKeyMsg->GetRepeat() )
                    handled = fGUIManager->IHandleKeyEvt( pfGameGUIMgr::kKeyDown, pKeyMsg->GetKeyCode(), fModifiers );
                else
                    handled = fGUIManager->IHandleKeyEvt( pfGameGUIMgr::kKeyRepeat, pKeyMsg->GetKeyCode(), fModifiers );

                if (pKeyMsg->GetKeyChar())
                    handled |= fGUIManager->IHandleKeyPress( pKeyMsg->GetKeyChar(), fModifiers );
            }
            else
                handled = fGUIManager->IHandleKeyEvt( pfGameGUIMgr::kKeyUp, pKeyMsg->GetKeyCode(), fModifiers );
        }

        // We need to do early interception of a screenshot request, since they want
        // us to be able to take screen shots while in a modal GUI... whee
        // Also, this should only be run if the dialog didn't handle the command in
        // the first place (taking screenshots while the user is typing would be
        // awkward) and we must do it on key down because the key binding routines
        // also trigger on key-down and we don't want to be taking screen shots when
        // the user re-binds the screenshot command.
        // HACK HACK HACK
        if ((!handled) && (pKeyMsg->GetKeyDown()) && !pKeyMsg->GetKeyChar())
        {
            const plKeyBinding* keymap = plInputInterfaceMgr::GetInstance()->FindBindingByConsoleCmd("Game.KITakePicture");
            if (keymap)
            {
                unsigned keyFlags = 0;
                if (pKeyMsg->GetCtrlKeyDown())
                    keyFlags |= plKeyCombo::kCtrl;
                if (pKeyMsg->GetShiftKeyDown())
                    keyFlags |= plKeyCombo::kShift;
                plKeyCombo combo(pKeyMsg->GetKeyCode(), keyFlags);
                if ((keymap->GetKey1().IsSatisfiedBy(combo)) || (keymap->GetKey2().IsSatisfiedBy(combo)))
                {
                    // tell the KI to take the shot
                    plConsoleMsg * consoleMsg = new plConsoleMsg;
                    consoleMsg->SetCmd(plConsoleMsg::kExecuteLine);
                    consoleMsg->SetString("Game.KITakePicture");
                    consoleMsg->Send(nil, true);
                }
            }
        }

        bool modal = fGUIManager->IModalBlocking();
        return handled || modal; // we "handle" it if we are modal, even if it didn't do anything
    }

    plMouseEventMsg *pMouseMsg = plMouseEventMsg::ConvertNoRef( pMsg );
    if( pMouseMsg && fManager->IsClickEnabled() )
    {
        if( pMouseMsg->GetButton() == kLeftButtonDown )
        {
            handled = fGUIManager->IHandleMouse( pfGameGUIMgr::kMouseDown, pMouseMsg->GetXPos(), pMouseMsg->GetYPos(), fModifiers, &fCurrentCursor );
            if (handled)
                fButtonState |= kLeftButtonDown;
        }
        else if( pMouseMsg->GetButton() == kLeftButtonUp )
        {
            handled = fGUIManager->IHandleMouse( pfGameGUIMgr::kMouseUp, pMouseMsg->GetXPos(), pMouseMsg->GetYPos(), fModifiers, &fCurrentCursor );
            if ((handled) || (fButtonState & kLeftButtonDown)) // even if we didn't handle the mouse up, if we think the button is still down, we should clear our flag
                fButtonState &= ~kLeftButtonDown;
        }
        else if( pMouseMsg->GetButton() == kLeftButtonDblClk )
            handled = fGUIManager->IHandleMouse( pfGameGUIMgr::kMouseDblClick, pMouseMsg->GetXPos(), pMouseMsg->GetYPos(), fModifiers, &fCurrentCursor );
        else if( fButtonState & kLeftButtonDown )
            handled = fGUIManager->IHandleMouse( pfGameGUIMgr::kMouseDrag, pMouseMsg->GetXPos(), pMouseMsg->GetYPos(), fModifiers, &fCurrentCursor );
        else
            handled = fGUIManager->IHandleMouse( pfGameGUIMgr::kMouseMove, pMouseMsg->GetXPos(), pMouseMsg->GetYPos(), fModifiers, &fCurrentCursor );

        fHaveInterestingCursor = handled;
        return handled;
    }

    return false;
}   

uint32_t  pfGameUIInputInterface::GetCurrentCursorID() const
{
    if( fCurrentCursor == 0 )
    {
        if ( pfGameGUIMgr::GetInstance() )
            return pfGameGUIMgr::GetInstance()->GetDefaultCursor();
        else
            return kCursorUp;
    }

    return fCurrentCursor;
}

float pfGameUIInputInterface::GetCurrentCursorOpacity() const
{
    if ( pfGameGUIMgr::GetInstance() )
        return pfGameGUIMgr::GetInstance()->GetCursorOpacity();
    else
        return 1.f;
}

//////////////////////////////////////////////////////////////////////////////
//// Tag Stuff ///////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

extern pfGUITag gGUITags[];     // From pfGUITagDefs.cpp

//// GetDialogFromTag ////////////////////////////////////////////////////////

pfGUIDialogMod  *pfGameGUIMgr::GetDialogFromTag( uint32_t tagID )
{
    for (pfGUIDialogMod* dialog : fDialogs)
    {
        if (dialog->GetTagID() == tagID)
            return dialog;
    }

    return nil;
}

//// GetDialogFromString ////////////////////////////////////////////////////////

pfGUIDialogMod  *pfGameGUIMgr::GetDialogFromString( const char *name )
{
    for (pfGUIDialogMod* dialog : fDialogs)
    {
        if (stricmp(dialog->GetName(), name) == 0)
            return dialog;
    }

    return nil;
}

//// GetControlFromTag ///////////////////////////////////////////////////////

pfGUIControlMod *pfGameGUIMgr::GetControlFromTag( pfGUIDialogMod *dlg, uint32_t tagID )
{
    return dlg->GetControlFromTag( tagID );
}

//// GetNumTags //////////////////////////////////////////////////////////////

uint32_t          pfGameGUIMgr::GetNumTags()
{
    uint32_t      count;


    for( count = 0; gGUITags[ count ].fID != 0; count++ );
    return count;   
}

//// GetTag //////////////////////////////////////////////////////////////////

pfGUITag        *pfGameGUIMgr::GetTag( uint32_t tagIndex )
{
    uint32_t      count;

    
    for( count = 0; gGUITags[ count ].fID != 0; count++ );
    hsAssert( tagIndex < count, "Bad index to GetTag()" );
            
    return &gGUITags[ tagIndex ];
}

uint32_t      pfGameGUIMgr::GetHighestTag()
{
    uint32_t  i, id = 1;


    for( i = 0; gGUITags[ i ].fID != 0; i++ )
    {
        if( id < gGUITags[ i ].fID )
            id = gGUITags[ i ].fID;
    }

    return id;
}


void pfGameGUIMgr::SetAspectRatio(float aspectratio)
{
    float oldAspectRatio = fAspectRatio;

    // don't allow the aspectratio below 4:3
    float fourThree = 4.0f/3.0f;
    fAspectRatio = aspectratio < fourThree ? fourThree : aspectratio;

    if (fAspectRatio != oldAspectRatio)
    {
        // need to tell dialogs to update
        for (pfGUIDialogMod* dialog : fDialogs)
        {
            if (dialog)
                dialog->UpdateAspectRatio();
        }
    }
}

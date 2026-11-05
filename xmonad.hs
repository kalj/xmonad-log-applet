-- Sample xmonad.hs for xmonad-log-applet, for xmonad in a MATE session. Needs
-- xmonad and xmonad-contrib 0.17 or later, and the Haskell dbus package.
--
-- To make MATE start xmonad instead of its own window manager (Marco), run
--
--     gsettings set org.mate.session.required-components windowmanager xmonad
--
-- and log in again. mateConfig makes room for the panels and adds MATE key
-- bindings: mod-p for the Run Application dialog, mod-d for the main menu and
-- mod-shift-q for the log out dialog (instead of quitting xmonad).
--
-- mateConfig also tries to register xmonad with the session manager, which
-- fails with a "ServiceUnknown" error in the session log, at least on MATE
-- 1.26: it uses the D-Bus name org.mate.SessionManager, but MATE's session
-- manager is org.gnome.SessionManager. That is harmless. Registering under
-- the right name would make logging out hang, as xmonad doesn't answer the
-- session manager's end of session queries.
--
-- With another desktop, replace mateConfig with e.g. gnomeConfig from
-- XMonad.Config.Gnome, or desktopConfig from XMonad.Config.Desktop.

import XMonad
import XMonad.Config.Mate
import XMonad.Hooks.EwmhDesktops
import XMonad.Hooks.StatusBar.PP
import XMonad.Hooks.UrgencyHook

import Data.List (intercalate)

import qualified DBus as D
import qualified DBus.Client as D
import qualified XMonad.StackSet as S

main :: IO ()
main = do
  dbus <- D.connectSession
  getWellKnownName dbus
  -- mateConfig already includes the EWMH hooks, so keep its logHook and
  -- run ours after it. The urgency hook lets the log see urgent windows,
  -- for the applet's urgent workspace colour.
  xmonad $ withUrgencyHook NoUrgencyHook $ ewmhFullscreen mateConfig {
    logHook = logHook mateConfig >> dynamicLogWithPP (appletPP dbus)
  }

-- Take the bus name the applet listens to
getWellKnownName :: D.Client -> IO ()
getWellKnownName dbus = do
  _ <- D.requestName dbus (D.busName_ "org.xmonad.Log")
                [D.nameAllowReplacement, D.nameReplaceExisting, D.nameDoNotQueue]
  return ()

-- The applet's message format: each workspace as its state marker, name and
-- window ids ("*1:1234,5678"), separated by ';', then '|' and the title of
-- the focused window. Empty hidden workspaces are left out.
appletPP :: D.Client -> PP
appletPP dbus = def
  { ppCurrent       = ("*" ++)
  , ppVisible       = ("+" ++)
  , ppHidden        = ("-" ++)
  , ppUrgent        = ("!" ++)
  , ppRename        = \name ws -> name ++ ":" ++ windowIds ws
  , ppWsSep         = ";"
  , ppSep           = ";"
  , ppLayout        = const ""
  , ppTitle         = ("|" ++)
  , ppTitleSanitize = id
  , ppOutput        = sendUpdate dbus
  }
  where windowIds = intercalate "," . map show . S.integrate' . S.stack

sendUpdate :: D.Client -> String -> IO ()
sendUpdate dbus msg = D.emit dbus
  (D.signal (D.objectPath_ "/org/xmonad/Log") (D.interfaceName_ "org.xmonad.Log")
            (D.memberName_ "Update"))
  { D.signalBody = [D.toVariant msg] }

{-# LANGUAGE OverloadedStrings #-}

-- Sample xmonad.hs for xmonad-log-applet. Needs xmonad and xmonad-contrib
-- 0.17 or later, and the Haskell dbus and utf8-string packages.

import XMonad
import XMonad.Config.Gnome
import XMonad.Hooks.EwmhDesktops
import XMonad.Hooks.StatusBar.PP
import XMonad.Hooks.UrgencyHook
import XMonad.Util.NamedWindows
import XMonad.Util.WorkspaceCompare

import Codec.Binary.UTF8.String (encodeString)
import Data.List (intersperse)
import Data.Maybe (isJust, catMaybes)

import qualified Codec.Binary.UTF8.String as UTF8
import qualified DBus as D
import qualified DBus.Client as D
import qualified XMonad.StackSet as S

main :: IO ()
main = do
  dbus <- D.connectSession
  getWellKnownName dbus
  -- gnomeConfig already includes the EWMH hooks, so keep its logHook and
  -- run ours after it. The urgency hook lets readUrgents (below) see urgent
  -- windows, for the applet's urgent workspace colour.
  xmonad $ withUrgencyHook NoUrgencyHook $ ewmhFullscreen gnomeConfig {
    logHook = logHook gnomeConfig >> iconsAppletLog dbus
  }

--
-- dbus
--

getWellKnownName :: D.Client -> IO ()
getWellKnownName dbus = do
  _ <- D.requestName dbus (D.busName_ "org.xmonad.Log")
                [D.nameAllowReplacement, D.nameReplaceExisting, D.nameDoNotQueue]
  return ()

dbusOutput :: D.Client -> String -> IO ()
dbusOutput dbus str = do
    let signal = (D.signal (D.objectPath_ "/org/xmonad/Log") (D.interfaceName_ "org.xmonad.Log") (D.memberName_ "Update")) {
            D.signalBody = [D.toVariant (UTF8.decodeString str)]
        }
    D.emit dbus signal

--
-- iconsAppletLog based on dynamicLogWithPP
-- (in addition to workspace numbers, it also outputs window id-s)
--

iconsAppletLog :: D.Client -> X ()
iconsAppletLog dbus = iconsAppletLogString >>= io . (dbusOutput dbus)

iconsAppletLogString :: X String
iconsAppletLogString = do

    winset <- gets windowset
    urgents <- readUrgents
    sort' <- ppSort iconsAppletPP

    -- layout description
    let ld = description . S.layout . S.workspace . S.current $ winset

    -- workspace list
    let ws = iconsAppletWindowSet sort' urgents winset

    -- window title
    wt <- maybe (return "") (fmap show . getName) . S.peek $ winset

    -- run extra loggers, ignoring any that generate errors.
    extras <- mapM (flip catchX (return Nothing)) $ ppExtras iconsAppletPP

    return $ encodeString . sepBy (ppSep iconsAppletPP) . ppOrder iconsAppletPP $
                        [ ws
                        , ppLayout iconsAppletPP ld
                        , ppTitle iconsAppletPP wt
                        ]
                        ++ catMaybes extras

iconsAppletWindowSet :: WorkspaceSort -> [Window] -> WindowSet -> String
iconsAppletWindowSet sort' urgents s = sepBy ";" . map fmt . sort' $
            map S.workspace (S.current s : S.visible s) ++ S.hidden s
   where this     = S.currentTag s
         visibles = map (S.tag . S.workspace) (S.visible s)

         fmt w = printer iconsAppletPP (iconsAppletFormatWorkspace w)
          where printer | any (\x -> maybe False (== S.tag w) (S.findTag x s)) urgents  = ppUrgent
                        | S.tag w == this                                               = ppCurrent
                        | S.tag w `elem` visibles                                       = ppVisible
                        | isJust (S.stack w)                                            = ppHidden
                        | otherwise                                                     = ppHiddenNoWindows

iconsAppletFormatWorkspace :: S.Workspace WorkspaceId (Layout Window) Window -> String
iconsAppletFormatWorkspace w = (S.tag w) ++ ":" ++ (iconsAppletFormatStack . S.stack $ w)

iconsAppletFormatStack :: Maybe (S.Stack Window) -> String
iconsAppletFormatStack s = case s of
  Just value -> sepBy "," . map show $ (reverse (S.up value) ++ [S.focus value] ++ S.down value)
  Nothing -> ""

iconsAppletPP :: PP
iconsAppletPP = def {
    ppTitle    = ("|" ++)
  , ppCurrent  = ("*" ++)
  , ppVisible  = ("+" ++)
  , ppHidden   = ("-" ++)
  , ppUrgent   = ("!" ++)
  , ppLayout   = const ""
  , ppSep      = ";"
  }

sepBy :: String   -- ^ separator
      -> [String] -- ^ fields to output
      -> String
sepBy sep = concat . intersperse sep . filter (not . null)

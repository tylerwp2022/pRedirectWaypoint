/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center             */
/*    FILE: RedirectWaypoint_Info.cpp                       */
/*    DATE: 2026                                            */
/************************************************************/

#include <cstdlib>
#include <iostream>
#include "RedirectWaypoint_Info.h"
#include "ColorParse.h"
#include "ReleaseInfo.h"

using namespace std;

//----------------------------------------------------------------
// Procedure: showSynopsis

void showSynopsis()
{
  blk("SYNOPSIS:                                                       ");
  blk("------------------------------------                            ");
  blk("  pRedirectWaypoint is a shoreside dispatcher for the           ");
  blk("  pMarineViewer click-to-redirect workflow, for Aquaticus       ");
  blk("  capture-the-flag (CTF) or high-value-target (HVT) missions.    ");
  blk("  It holds the                                                  ");
  blk("  operator's current vehicle selection (team + index, set by    ");
  blk("  the patched pMarineViewer b/r/1..9 keys) and, on each map      ");
  blk("  click (RDR_CLICK), pokes the per-vehicle REDIR_ACTIVE and      ");
  blk("  REDIR_UPDATE command variables for the selected boat.         ");
  blk("                                                                ");
  blk("  It performs no geodesy and owns no behavior logic -- it only  ");
  blk("  maps {selection + click} to a per-vehicle waypoint command    ");
  blk("  that uFldShoreBroker qbridges down to the vehicle.            ");
}

//----------------------------------------------------------------
// Procedure: showExampleConfigAndExit

void showExampleConfigAndExit()
{
  blk("=============================================================== ");
  blk("pRedirectWaypoint Example MOOS Configuration                    ");
  blk("=============================================================== ");
  blk("                                                                ");
  blk("ProcessConfig = pRedirectWaypoint                               ");
  blk("{                                                               ");
  blk("  AppTick   = 4                                                 ");
  blk("  CommsTick = 4                                                 ");
  blk("                                                                ");
  blk("  // Ordered rosters. List position == selection index (1-based)");
  blk("  team_blue = blue_one, blue_two, blue_three                    ");
  blk("  team_red  = red_one,  red_two,  red_three                     ");
  blk("                                                                ");
  blk("  // Waypoint tuning, applied to every dispatched waypoint      ");
  blk("  capture_radius = 10.0                                         ");
  blk("  slip_radius    = 15.0                                         ");
  blk("  speed          = 2.0                                          ");
  blk("                                                                ");
  blk("  // Radius (m) of the control halo drawn around each           ");
  blk("  // controlled boat                                            ");
  blk("  control_circle_radius = 10.0                                  ");
  blk("                                                                ");
  blk("  // Variable name overrides (defaults shown)                   ");
  blk("  // click_var        = RDR_CLICK                               ");
  blk("  // sel_team_var     = RDR_SEL_TEAM                            ");
  blk("  // sel_index_var    = RDR_SEL_INDEX                           ");
  blk("  // mode_toggle_var  = RDR_CLICK_MODE_TOGGLE                   ");
  blk("  // mode_state_var   = RDR_CLICK_MODE                          ");
  blk("  // untag_toggle_var = RDR_UNTAG_TOGGLE                        ");
  blk("  // untag_var        = REDIR_ALLOW_UNTAG                       ");
  blk("  // arrived_var      = REDIR_ARRIVED                           ");
  blk("  // controlled_var   = REDIR_CONTROLLED                        ");
  blk("  // active_var       = REDIR_ACTIVE                            ");
  blk("  // update_var       = REDIR_UPDATE                            ");
  blk("                                                                ");
  blk("  debug = false                                                 ");
  blk("}                                                               ");
  exit(0);
}

//----------------------------------------------------------------
// Procedure: showInterfaceAndExit

void showInterfaceAndExit()
{
  blk("=============================================================== ");
  blk("pRedirectWaypoint INTERFACE                                     ");
  blk("=============================================================== ");
  blk("                                                                ");
  blk("SUBSCRIPTIONS:                                                  ");
  blk("------------------------------------                            ");
  blk("  RDR_SEL_TEAM           (string)  blue | red                   ");
  blk("  RDR_SEL_INDEX          (string)  1..N                         ");
  blk("  RDR_CLICK_MODE_TOGGLE  (string)  changing value (edge)        ");
  blk("  RDR_UNTAG_TOGGLE       (string)  changing value (edge)        ");
  blk("  RDR_CLICK              (string)  x=<m>,y=<m>  (local grid)     ");
  blk("  NODE_REPORT[_*]        (string)  live positions (halos)       ");
  blk("  TAGGED_VEHICLES        (string)  CSV of tagged boats          ");
  blk("  TAGGED_<VNAME>         (string)  true | false                 ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  RDR_CLICK_MODE             (string) true | false  (latch)     ");
  blk("  REDIR_CONTROLLED_<VNAME>   (string) true | false              ");
  blk("  REDIR_ACTIVE_<VNAME>       (string) true | false              ");
  blk("  REDIR_UPDATE_<VNAME>       (string) points=x,y#capture_radius=..");
  blk("  REDIR_ALLOW_UNTAG_<VNAME>  (string) true | false              ");
  blk("  REDIR_ARRIVED_<VNAME>      (string) false (clear-only; set    ");
  blk("                             true on the vehicle by the         ");
  blk("                             waypt_redirect endflag)            ");
  blk("  VIEW_CIRCLE                (string) control halos              ");
  exit(0);
}

//----------------------------------------------------------------
// Procedure: showReleaseInfoAndExit

void showReleaseInfoAndExit()
{
  showReleaseInfo("pRedirectWaypoint", "gpl");
  exit(0);
}

//----------------------------------------------------------------
// Procedure: showHelpAndExit

void showHelpAndExit()
{
  blk("                                                                ");
  blu("=============================================================== ");
  blu("Usage: pRedirectWaypoint file.moos [OPTIONS]                    ");
  blu("=============================================================== ");
  blk("                                                                ");
  showSynopsis();
  blk("                                                                ");
  blk("Options:                                                        ");
  mag("  --alias","=<ProcessName>                                      ");
  blk("      Launch pRedirectWaypoint with the given process name.     ");
  mag("  --example, -e","                                              ");
  blk("      Display example MOOS configuration block.                 ");
  mag("  --help, -h","                                                 ");
  blk("      Display this help message.                                ");
  mag("  --interface, -i","                                            ");
  blk("      Display MOOS publications and subscriptions.              ");
  mag("  --version,-v","                                               ");
  blk("      Display the release version of pRedirectWaypoint.         ");
  exit(0);
}

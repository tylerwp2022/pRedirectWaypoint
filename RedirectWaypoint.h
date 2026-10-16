/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center             */
/*    FILE: RedirectWaypoint.h                              */
/*    DATE: 2026                                            */
/*                                                          */
/*  Shoreside dispatcher for the pMarineViewer              */
/*  click-to-redirect feature. For Aquaticus capture-the-   */
/*  flag (CTF) or high-value-target (HVT) missions.         */
/*                                                          */
/*  Operating model:                                        */
/*   - b / r selects the CONTROLLED team. That whole team   */
/*     is put under operator control (each boat station-    */
/*     holds at its current position via REDIR_CONTROLLED). */
/*     The other team is released to autonomy               */
/*     (REDIR_CONTROLLED=false).                            */
/*   - 1 / 2 / 3 selects which boat on the controlled team  */
/*     the next click commands.                             */
/*   - w toggles waypoint-click mode. Clicks only redirect  */
/*     when the mode latch is on.                           */
/*   - a left-click (RDR_CLICK) sends the selected boat to  */
/*     the clicked point (REDIR_ACTIVE + REDIR_UPDATE). On  */
/*     arrival the boat reverts to station-hold AT the      */
/*     clicked point (handled on the vehicle by the bhv).   */
/*                                                          */
/*  Thin wrapper: no geodesy, no behavior logic. Only maps  */
/*  operator intent to the per-vehicle command variables    */
/*  that uFldShoreBroker qbridges down to each vehicle.     */
/************************************************************/

#ifndef REDIRECT_WAYPOINT_HEADER
#define REDIRECT_WAYPOINT_HEADER

#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>
#include "NodeRecord.h"
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

class RedirectWaypoint : public AppCastingMOOSApp
{
 public:
  RedirectWaypoint();
  ~RedirectWaypoint() {}

 protected: // Standard MOOSApp / AppCasting
  bool OnNewMail(MOOSMSG_LIST &NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();
  void registerVariables();
  bool buildReport();

 protected: // Implementation helpers
  bool   handleConfigTeamVehicles(std::string team, std::string csv);
  void   handleSelectTeam(std::string team);
  void   handleSelectIndex(std::string idx_str);
  void   handleClickModeToggle();
  void   handleUntagToggle();
  bool   handleClick(std::string click_str);
  bool   handleHeadingClick(std::string click_str);
  void   recomputeControl();
  // Wake exactly the selected boat of a controlled team (drop its
  // manual override) and re-park every boat this app previously woke.
  // Never touches boats we did not wake, so autonomously-deployed
  // vehicles are left alone.
  void   reconcileWake();
  void   setOverride(const std::string& vname, bool override_on);
  // Erase a boat's station_hold keep-in/out/hiber polygons from the
  // viewer. Needed on release: forcing manual override parks the helm
  // before BHV_StationKeep can clear its own visuals, so they linger.
  void   clearStationKeepViz(const std::string& vname);
  // True if the game is deployed for this boat (DEPLOY pressed). On
  // release, woken boats are restored to override=!isDeployed(): parked
  // if never deployed, autonomous if deployed.
  bool   isDeployed(const std::string& vname) const;
  void   dispatchWaypoint(const std::string& vname, double x, double y,
                          bool turn, double heading);
  void   postPendingMarker(const std::string& vname, double x, double y);
  void   clearPendingMarker(const std::string& vname);
  void   postHeadingArrow(const std::string& vname, double x, double y,
                          double heading);
  void   handleMailNodeReport(std::string report);
  void   handleMailTaggedVehicles(std::string csv);
  void   handleMailTagged(std::string vname, bool tagged);
  void   updateControlCircles();
  void   postControlCircle(const std::string& vname, bool selected);
  void   clearControlCircle(const std::string& vname);
  std::string selectedVName() const;
  const std::vector<std::string>& teamRoster(std::string team) const;
  void   debugLog(const std::string& line);

 private: // Configuration
  std::vector<std::string> m_blue_vehicles;  // index 1 -> element 0
  std::vector<std::string> m_red_vehicles;

  // MOOS variable names (overridable in config)
  std::string m_click_var;        // RDR_CLICK
  std::string m_sel_team_var;     // RDR_SEL_TEAM
  std::string m_sel_index_var;    // RDR_SEL_INDEX
  std::string m_mode_toggle_var;  // RDR_CLICK_MODE_TOGGLE
  std::string m_mode_state_var;   // RDR_CLICK_MODE  (published latch)
  std::string m_untag_toggle_var; // RDR_UNTAG_TOGGLE
  std::string m_active_var;       // REDIR_ACTIVE     (suffixed)
  std::string m_update_var;       // REDIR_UPDATE     (suffixed)
  std::string m_controlled_var;   // REDIR_CONTROLLED (suffixed)
  std::string m_untag_var;        // REDIR_ALLOW_UNTAG (suffixed)
  std::string m_arrived_var;      // REDIR_ARRIVED (suffixed; cleared by
                                  // this app, set true on the vehicle by
                                  // the waypt_redirect endflag)
  std::string m_heading_click_var; // RDR_CLICK_HDG (right-click: arm heading)
  std::string m_heading_var;      // REDIR_HEADING (suffixed; numeric compass
                                  // heading consumed by BHV_TurnInPlace)
  std::string m_turn_var;         // REDIR_TURN (suffixed; gates the on-arrival
                                  // turn-in-place: true only on the heading path)

  // Waypoint tuning passed through in REDIR_UPDATE
  double m_capture_radius;
  double m_slip_radius;
  double m_speed;
  double m_control_circle_radius;  // VIEW_CIRCLE radius for control halo

 private: // State
  std::string  m_controlled_team; // "", "blue", or "red" (last selected; click target)
  unsigned int m_sel_index;       // 1-based; 0 = none
  bool         m_click_mode;      // waypoint-click latch

  // Heading gesture (right-click then left-click): a right-click drops
  // the waypoint and arms "awaiting heading"; the next left-click sets the
  // heading = direction from that waypoint to the second click, and the
  // boat turns-in-place to it on arrival.
  bool         m_awaiting_heading;
  double       m_pending_x;
  double       m_pending_y;
  std::string  m_pending_vname;   // boat captured at right-click time
  std::string  m_last_heading;    // appcast readout

  // Teams currently latched under control. While click mode is ON,
  // pressing b/r ADDS that team to this set (and it stays until click
  // mode is turned OFF, which clears the set). This lets the operator
  // control both teams at once.
  std::set<std::string> m_controlled_teams;

  // Live boat positions for drawing control circles that follow the
  // vehicles. Keyed by lowercased vname.
  std::map<std::string, NodeRecord> m_node_records;
  // Tagged (disabled) boats -- excluded from control-circle drawing.
  std::set<std::string> m_tagged;
  // Labels of control circles currently drawn, so we can clear ones
  // that should no longer appear.
  std::set<std::string> m_drawn_circles;

  // Boats (by vname) currently allowed to auto-untag while
  // controlled. Per-boat, keyed to the b/r/123 selection.
  std::set<std::string> m_untag_allowed;

  // Boats this app has woken (posted MOOS_MANUAL_OVERRIDE=false).
  // Only these are re-parked on release/reselect, so a boat driving
  // itself under autonomy is never frozen by us. See reconcileWake().
  std::set<std::string> m_awake;

  // Deploy state, so release can restore a boat to its pre-control
  // condition. m_deploy_all tracks DEPLOY_ALL (the viewer Deploy/Pause
  // buttons); m_deploy_vehicle holds any per-vehicle DEPLOY_<V> override.
  bool m_deploy_all;
  std::map<std::string, bool> m_deploy_vehicle;

  // Appcast bookkeeping
  unsigned int m_clicks_received;
  unsigned int m_waypoints_sent;
  std::string  m_last_target;
  std::string  m_last_point;
  std::string  m_last_warning;

  // Debug circular buffer (house style)
  bool                    m_debug;
  std::list<std::string>  m_debug_buffer;
  unsigned int            m_debug_buffer_max;
};

#endif

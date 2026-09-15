/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center             */
/*    FILE: RedirectWaypoint.cpp                            */
/*    DATE: 2026                                            */
/************************************************************/

#include <iterator>
#include <cstdlib>
#include "MBUtils.h"
#include "NodeRecordUtils.h"
#include "ACTable.h"
#include "RedirectWaypoint.h"

using namespace std;

//---------------------------------------------------------
// Constructor

RedirectWaypoint::RedirectWaypoint()
{
  // Variable-name defaults. These line up with:
  //   - patched pMarineViewer (RDR_SEL_TEAM / RDR_SEL_INDEX /
  //     RDR_CLICK_MODE_TOGGLE)
  //   - left_context poke in meta_shoreside.moos (RDR_CLICK)
  //   - uFldShoreBroker qbridges (REDIR_ACTIVE / REDIR_UPDATE /
  //     REDIR_CONTROLLED)
  //   - waypt_redirect + station_hold behaviors on the vehicle
  m_click_var       = "RDR_CLICK";
  m_sel_team_var    = "RDR_SEL_TEAM";
  m_sel_index_var   = "RDR_SEL_INDEX";
  m_mode_toggle_var = "RDR_CLICK_MODE_TOGGLE";
  m_mode_state_var  = "RDR_CLICK_MODE";
  m_untag_toggle_var = "RDR_UNTAG_TOGGLE";
  m_active_var      = "REDIR_ACTIVE";
  m_update_var      = "REDIR_UPDATE";
  m_controlled_var  = "REDIR_CONTROLLED";
  m_untag_var       = "REDIR_ALLOW_UNTAG";
  m_arrived_var     = "REDIR_ARRIVED";

  m_capture_radius = 10.0;
  m_slip_radius    = 15.0;
  m_speed          = 2.0;
  m_control_circle_radius = 10.0;

  m_controlled_team = "";
  m_sel_index       = 0;
  m_click_mode      = false;

  m_clicks_received = 0;
  m_waypoints_sent  = 0;

  m_deploy_all = false;

  m_debug = false;
  m_debug_buffer_max = 20;
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool RedirectWaypoint::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p=NewMail.begin(); p!=NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key  = msg.GetKey();
    string sval = msg.GetString();

    if(key == m_sel_team_var)
      handleSelectTeam(tolower(sval));
    else if(key == m_sel_index_var) {
      // The index can arrive as a string ("1") or, depending on how
      // the viewer's pending-pair post is typed, as a double (1.0).
      // If the string field is empty, fall back to the numeric field
      // so either posting type works.
      if(sval == "")
        sval = doubleToStringX(msg.GetDouble(), 0);
      handleSelectIndex(sval);
    }
    else if(key == m_mode_toggle_var)
      handleClickModeToggle();
    else if(key == m_untag_toggle_var)
      handleUntagToggle();
    else if(key == m_click_var)
      handleClick(sval);
    else if((key == "DEPLOY_ALL") || (key == "DEPLOY")) {
      m_deploy_all = (tolower(sval) == "true");
    }
    else if(strBegins(key, "DEPLOY_")) {
      string vname = tolower(key.substr(7));
      m_deploy_vehicle[vname] = (tolower(sval) == "true");
    }
    else if((key == "NODE_REPORT") || strBegins(key, "NODE_REPORT_"))
      handleMailNodeReport(sval);
    else if(key == "TAGGED_VEHICLES")
      handleMailTaggedVehicles(sval);
    else if(strBegins(key, "TAGGED_")) {
      string vname = tolower(key.substr(7));
      handleMailTagged(vname, (tolower(sval) == "true"));
    }
  }
  return(true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool RedirectWaypoint::OnConnectToServer()
{
  registerVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()  (event-driven; iterate only appcasts)

bool RedirectWaypoint::Iterate()
{
  AppCastingMOOSApp::Iterate();
  updateControlCircles();   // redraw control halos at current positions
  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()

bool RedirectWaypoint::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.EnableVerbatimQuoting(false);
  m_MissionReader.GetConfiguration(GetAppName(), sParams);

  STRING_LIST::iterator p;
  for(p=sParams.begin(); p!=sParams.end(); p++) {
    string orig  = *p;
    string line  = *p;
    string param = tolower(biteStringX(line, '='));
    string value = line;

    bool handled = false;
    if(param == "team_blue")
      handled = handleConfigTeamVehicles("blue", value);
    else if(param == "team_red")
      handled = handleConfigTeamVehicles("red", value);
    else if(param == "click_var") {
      m_click_var = value; handled = true; }
    else if(param == "sel_team_var") {
      m_sel_team_var = value; handled = true; }
    else if(param == "sel_index_var") {
      m_sel_index_var = value; handled = true; }
    else if(param == "mode_toggle_var") {
      m_mode_toggle_var = value; handled = true; }
    else if(param == "mode_state_var") {
      m_mode_state_var = value; handled = true; }
    else if(param == "untag_toggle_var") {
      m_untag_toggle_var = value; handled = true; }
    else if(param == "untag_var") {
      m_untag_var = value; handled = true; }
    else if(param == "arrived_var") {
      m_arrived_var = value; handled = true; }
    else if(param == "active_var") {
      m_active_var = value; handled = true; }
    else if(param == "update_var") {
      m_update_var = value; handled = true; }
    else if(param == "controlled_var") {
      m_controlled_var = value; handled = true; }
    else if(param == "capture_radius")
      handled = setPosDoubleOnString(m_capture_radius, value);
    else if(param == "slip_radius")
      handled = setPosDoubleOnString(m_slip_radius, value);
    else if(param == "speed")
      handled = setPosDoubleOnString(m_speed, value);
    else if(param == "control_circle_radius")
      handled = setPosDoubleOnString(m_control_circle_radius, value);
    else if(param == "debug")
      handled = setBooleanOnString(m_debug, value);

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  // Publish the initial click-mode latch so downstream apps and
  // the appcast agree on the starting state (off).
  Notify(m_mode_state_var, "false");

  registerVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: registerVariables()

void RedirectWaypoint::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register(m_sel_team_var, 0);
  Register(m_sel_index_var, 0);
  Register(m_mode_toggle_var, 0);
  Register(m_untag_toggle_var, 0);
  Register(m_click_var, 0);
  Register("NODE_REPORT", 0);
  Register("NODE_REPORT_LOCAL", 0);
  Register("TAGGED_VEHICLES", 0);

  // Deploy state -- so releasing a boat can restore it to how it was
  // before we took it: parked if Deploy was never pressed, autonomous
  // if it was. DEPLOY_ALL is what the viewer Deploy/Pause buttons post.
  Register("DEPLOY_ALL", 0);
  Register("DEPLOY", 0);
  for(int t=0; t<2; t++) {
    const std::vector<std::string>& roster = teamRoster((t==0)?"blue":"red");
    for(unsigned int i=0; i<roster.size(); i++)
      Register("DEPLOY_" + toupper(roster[i]), 0);
  }
}

//---------------------------------------------------------
// Procedure: handleConfigTeamVehicles()
//   csv is an ordered list; list position is the 1-based index.

bool RedirectWaypoint::handleConfigTeamVehicles(string team, string csv)
{
  vector<string> names = parseString(csv, ',');
  for(unsigned int i=0; i<names.size(); i++)
    names[i] = stripBlankEnds(names[i]);
  if(names.empty())
    return(false);

  if(team == "blue")
    m_blue_vehicles = names;
  else if(team == "red")
    m_red_vehicles = names;
  else
    return(false);
  return(true);
}

//---------------------------------------------------------
// Procedure: teamRoster()

const vector<string>& RedirectWaypoint::teamRoster(string team) const
{
  if(team == "blue")
    return(m_blue_vehicles);
  return(m_red_vehicles);
}

//---------------------------------------------------------
// Procedure: handleSelectTeam()
//   Sets the click-target team and resets the boat index. Selection
//   alone does NOT change control -- 'w' toggles control for the
//   selected team. This lets the operator switch the click target
//   between two already-controlled teams without releasing either.

void RedirectWaypoint::handleSelectTeam(string team)
{
  if((team != "blue") && (team != "red")) {
    m_last_warning = "Bad team value: " + team;
    debugLog(m_last_warning);
    return;
  }
  m_controlled_team = team;   // click target (which boats 1/2/3 pick from)
  m_sel_index = 0;            // force a fresh 1/2/3 pick for the new team

  // No control change on selection -- 'w' controls. Do NOT recompute
  // here: control membership is unchanged, and recomputeControl()
  // clears REDIR_ACTIVE for all boats, which would cancel an in-flight
  // click-waypoint on the other controlled team when switching target.

  bool held = (m_controlled_teams.count(team) > 0);
  debugLog("Click target: " + team +
           (held ? " (controlled)" : " (not controlled; press w to control)"));

  // Selecting a team resets the boat index; wake whatever boat is now
  // selected on a controlled team (none, until a number is pressed).
  reconcileWake();
}

//---------------------------------------------------------
// Procedure: setOverride()
//   Post MOOS_MANUAL_OVERRIDE for one boat. Both the correct and the
//   legacy one-R spelling are bridged by uFldShoreBroker, so post both
//   so whichever pMarinePID/iM1_8 build is running obeys it.

void RedirectWaypoint::setOverride(const string& vname, bool override_on)
{
  string up  = toupper(vname);
  string val = override_on ? "true" : "false";
  Notify("MOOS_MANUAL_OVERRIDE_" + up, val);
  Notify("MOOS_MANUAL_OVERIDE_" + up, val);
}

//---------------------------------------------------------
// Procedure: clearStationKeepViz()
//   BHV_StationKeep draws three polygons per boat -- <vname>:station-
//   keep-out / -in / -hiber. On release we force manual override, which
//   parks the helm before the behavior can erase its own visuals, so the
//   keep-out ring lingers on the map. Post each label inactive to remove
//   it. Removal is by label, so a minimal spec at the boat's last known
//   position is enough; geometry is ignored.

void RedirectWaypoint::clearStationKeepViz(const string& vname)
{
  double x = 0, y = 0;
  map<string, NodeRecord>::iterator p = m_node_records.find(tolower(vname));
  if(p != m_node_records.end()) {
    x = p->second.getX();
    y = p->second.getY();
  }
  const char* suffix[3] = {"station-keep-out","station-keep-in","station-keep-hiber"};
  for(int i=0; i<3; i++) {
    string spec = "radial:: x=" + doubleToStringX(x,1) +
                  ",y=" + doubleToStringX(y,1) +
                  ",radius=1,pts=8,label=" + vname + ":" + suffix[i] +
                  ",active=false";
    Notify("VIEW_POLYGON", spec);
  }
  debugLog("Cleared station-keep visuals for " + vname);
}

//---------------------------------------------------------
// Procedure: reconcileWake()
//   Exactly one boat may be "awake" (manual override dropped): the
//   selected boat of the click-target team, and only while that team
//   is actually controlled. Every boat THIS APP previously woke and
//   that is no longer that boat is re-parked (override restored).
//
//   Only boats in m_awake are ever re-parked, so a vehicle driving
//   itself under autonomy -- one we never woke -- is never frozen here.

void RedirectWaypoint::reconcileWake()
{
  // Wake the selected boat of the click-target team, if that team is
  // controlled. A woken boat STAYS awake -- it finishes any click-
  // waypoint and then station-holds at the destination -- even after
  // you select off it. Boats are only ever re-parked when their team is
  // released (see recomputeControl), never on reselect.
  if(m_controlled_teams.count(m_controlled_team) == 0)
    return;

  string wake = selectedVName();
  if((wake != "") && (m_awake.count(wake) == 0)) {
    setOverride(wake, false);
    m_awake.insert(wake);
    debugLog("Woke selected boat " + wake + " (manual override dropped)");
  }
}

//---------------------------------------------------------
// Procedure: isDeployed()
//   Per-vehicle DEPLOY_<V> if we have seen one, else the global
//   DEPLOY_ALL state (what the viewer Deploy/Pause buttons drive).

bool RedirectWaypoint::isDeployed(const string& vname) const
{
  map<string,bool>::const_iterator it = m_deploy_vehicle.find(tolower(vname));
  if(it != m_deploy_vehicle.end())
    return(it->second);
  return(m_deploy_all);
}

//---------------------------------------------------------
// Procedure: recomputeControl()
//   A team's boats are held iff that team is in the controlled-teams
//   set. Controlled == held + clickable. Boats not in a controlled
//   team are released to autonomy. Called whenever the set changes.

void RedirectWaypoint::recomputeControl()
{
  for(int t=0; t<2; t++) {
    string team = (t==0) ? "blue" : "red";
    const vector<string>& roster = teamRoster(team);
    bool hold = (m_controlled_teams.count(team) > 0);
    for(unsigned int i=0; i<roster.size(); i++) {
      string up = toupper(roster[i]);
      Notify(m_controlled_var + "_" + up, hold ? "true" : "false");
      // Manual override is deliberately NOT dropped for the whole team
      // here. Doing so woke every boat on a controlled team (motors live)
      // as if DEPLOY had been pressed, even though only one was selected.
      // reconcileWake() below wakes a boat only when it is selected; a
      // woken boat then stays awake until its team is released.
      if(!hold) {
        // Releasing this boat to autonomy: clear any click-waypoint,
        // its arrival latch, and reset its untag-allow. Boats that
        // REMAIN controlled are left untouched so an in-flight click
        // on the other team is not cancelled when toggling this team.
        Notify(m_active_var + "_" + up, "false");
        Notify(m_arrived_var + "_" + up, "false");
        Notify(m_untag_var + "_" + up, "false");
        m_untag_allowed.erase(roster[i]);
        // Restore a boat we woke to its pre-control state: parked
        // (override on) if the game was never deployed, or driving its
        // autonomous behaviors (override off) if Deploy has been pressed.
        // Boats we never woke are left untouched, so an autonomously
        // deployed vehicle is never frozen by us.
        if(m_awake.count(roster[i])) {
          setOverride(roster[i], !isDeployed(roster[i]));
          clearStationKeepViz(roster[i]);
          m_awake.erase(roster[i]);
          debugLog("Released " + roster[i] + " -> " +
                   (isDeployed(roster[i]) ? "autonomy (override off)"
                                          : "parked (override on)"));
        }
      }
    }
  }

  // Wake the selected controlled boat (if any). Boats stay awake until
  // their team is released, which is handled in the !hold branch above.
  reconcileWake();
}

//---------------------------------------------------------
// Procedure: handleMailNodeReport()
//   Track live boat positions so control circles can follow them.

void RedirectWaypoint::handleMailNodeReport(string report)
{
  NodeRecord record = string2NodeRecord(report);
  string vname = tolower(record.getName());
  if(vname != "")
    m_node_records[vname] = record;
}

//---------------------------------------------------------
// Procedure: handleMailTaggedVehicles()
//   TAGGED_VEHICLES is the authoritative comma list of tagged boats.

void RedirectWaypoint::handleMailTaggedVehicles(string csv)
{
  m_tagged.clear();
  vector<string> names = parseString(csv, ',');
  for(unsigned int i=0; i<names.size(); i++) {
    string v = tolower(stripBlankEnds(names[i]));
    if(v != "")
      m_tagged.insert(v);
  }
}

//---------------------------------------------------------
// Procedure: handleMailTagged()
//   TAGGED_<VNAME> = true/false for a single boat.

void RedirectWaypoint::handleMailTagged(string vname, bool tagged)
{
  if(tagged)
    m_tagged.insert(vname);
  else
    m_tagged.erase(vname);
}

//---------------------------------------------------------
// Procedure: updateControlCircles()
//   Each iterate: draw a magenta VIEW_CIRCLE around every boat on a
//   controlled team (filled if it is the currently selected boat,
//   outline otherwise). Circles follow the boats since they are
//   redrawn at current position each tick. Boats no longer
//   controlled have their circle cleared. Tagged controlled boats
//   keep their control circle (the tag manager's circle shows too).

void RedirectWaypoint::updateControlCircles()
{
  string sel = selectedVName();

  // Set of boats that SHOULD have a circle this tick.
  set<string> want;

  for(int t=0; t<2; t++) {
    string team = (t==0) ? "blue" : "red";
    if(m_controlled_teams.count(team) == 0)
      continue;
    const vector<string>& roster = teamRoster(team);
    for(unsigned int i=0; i<roster.size(); i++) {
      string v = roster[i];
      // Tagged boats lose the control circle (tagged = disabled,
      // applies to explosions and normal tags alike).
      if(m_tagged.count(v) > 0)
        continue;
      // Need a known position to draw.
      if(m_node_records.find(v) == m_node_records.end())
        continue;
      postControlCircle(v, (v == sel));
      want.insert(v);
    }
  }

  // Clear circles that were drawn before but are no longer wanted.
  set<string> stale;
  set<string>::iterator d;
  for(d=m_drawn_circles.begin(); d!=m_drawn_circles.end(); d++)
    if(want.count(*d) == 0)
      stale.insert(*d);
  for(d=stale.begin(); d!=stale.end(); d++)
    clearControlCircle(*d);

  m_drawn_circles = want;
}

//---------------------------------------------------------
// Procedure: postControlCircle()
//   Magenta circle at the boat's current position. Filled when the
//   boat is the selected one, outline-only otherwise. Label is keyed
//   per-vehicle so repeated posts update in place (and can be cleared
//   by label later).

void RedirectWaypoint::postControlCircle(const string& vname, bool selected)
{
  map<string, NodeRecord>::iterator p = m_node_records.find(vname);
  if(p == m_node_records.end())
    return;
  double x = p->second.getX();
  double y = p->second.getY();

  string spec;
  spec += "x=" + doubleToStringX(x,2);
  spec += ",y=" + doubleToStringX(y,2);
  spec += ",radius=" + doubleToStringX(m_control_circle_radius,1);
  spec += ",duration=0";                 // persists until updated/cleared
  spec += ",label=" + vname + "_ctrl";   // per-vehicle label
  spec += ",edge_color=magenta";
  spec += ",edge_size=2";
  spec += ",vertex_size=0";
  if(selected) {
    spec += ",fill_color=magenta";
    spec += ",fill_transparency=0.3";
  }
  else {
    spec += ",fill_transparency=0.0";    // outline only
  }
  Notify("VIEW_CIRCLE", spec);
}

//---------------------------------------------------------
// Procedure: clearControlCircle()
//   Remove a previously drawn control circle by re-posting it as
//   inactive (same label).

void RedirectWaypoint::clearControlCircle(const string& vname)
{
  // Some pMarineViewer builds reject a VIEW_CIRCLE that carries only
  // a label + active=false (no geometry) as "unhandled mail". Post a
  // full, parseable circle spec at the boat's last known position
  // with active=false so the viewer cleanly deactivates it by label.
  double x = 0, y = 0;
  map<string, NodeRecord>::iterator p = m_node_records.find(vname);
  if(p != m_node_records.end()) {
    x = p->second.getX();
    y = p->second.getY();
  }
  string spec;
  spec += "x=" + doubleToStringX(x,2);
  spec += ",y=" + doubleToStringX(y,2);
  spec += ",radius=" + doubleToStringX(m_control_circle_radius,1);
  spec += ",label=" + vname + "_ctrl";
  spec += ",active=false";
  Notify("VIEW_CIRCLE", spec);
}



void RedirectWaypoint::handleSelectIndex(string idx_str)
{
  int idx = atoi(idx_str.c_str());
  if(idx >= 1) {
    m_sel_index = (unsigned int)idx;
    debugLog("Index selected: " + uintToString(m_sel_index));
    // Wake the newly selected boat when its team is controlled.
    reconcileWake();
  }
  else {
    m_last_warning = "Bad index value: " + idx_str;
    debugLog(m_last_warning);
  }
}

//---------------------------------------------------------
// Procedure: handleClickModeToggle()
//   The keypress (and on-screen button) post a changing value;
//   this app owns the actual on/off latch and republishes it so
//   the operator can see state in the appcast / a scope field.

void RedirectWaypoint::handleClickModeToggle()
{
  // 'w' now toggles control for the CURRENTLY SELECTED team only.
  // No team selected -> do nothing, but tell the operator via NOTE.
  if(m_controlled_team == "") {
    m_last_warning = "Press b or r to select a team before pressing w";
    debugLog(m_last_warning);
    return;
  }

  bool now_controlled;
  if(m_controlled_teams.count(m_controlled_team)) {
    m_controlled_teams.erase(m_controlled_team);   // release this team
    now_controlled = false;
  }
  else {
    m_controlled_teams.insert(m_controlled_team);  // control this team
    now_controlled = true;
    // Newly controlled: clear any stale click-waypoint and arrival
    // latch so boats hold at current position (station_hold) rather
    // than resuming an old REDIR_ACTIVE / REDIR_ARRIVED state from a
    // previous control session.
    const vector<string>& roster = teamRoster(m_controlled_team);
    for(unsigned int i=0; i<roster.size(); i++) {
      Notify(m_active_var + "_" + toupper(roster[i]), "false");
      Notify(m_arrived_var + "_" + toupper(roster[i]), "false");
    }
  }

  // m_click_mode is retained only as a readout: true if ANY team is
  // currently controlled (kept for the existing RDR_CLICK_MODE scope).
  m_click_mode = !m_controlled_teams.empty();
  Notify(m_mode_state_var, boolToString(m_click_mode));

  recomputeControl();
  debugLog(m_controlled_team + " control -> " +
           (now_controlled ? "ON (held + clickable)" : "OFF (autonomy)"));
}

//---------------------------------------------------------
// Procedure: handleUntagToggle()
//   Per-boat: flips auto-untag-allow for the currently selected
//   boat and pokes REDIR_ALLOW_UNTAG_<VNAME>. Other boats are
//   untouched (per-boat state lives in m_untag_allowed). Default
//   is OFF for every boat (set when the team is selected), which
//   is the HVT "tagged = dead" behavior; toggling ON lets that
//   boat auto-return to untag (CTF style).

void RedirectWaypoint::handleUntagToggle()
{
  string vname = selectedVName();
  if(vname == "") {
    m_last_warning = "Untag toggle ignored -- no valid boat selected";
    debugLog(m_last_warning);
    return;
  }

  bool now_allowed;
  if(m_untag_allowed.count(vname)) {
    m_untag_allowed.erase(vname);
    now_allowed = false;
  }
  else {
    m_untag_allowed.insert(vname);
    now_allowed = true;
  }

  string up = toupper(vname);
  Notify(m_untag_var + "_" + up, boolToString(now_allowed));
  debugLog("Untag-allow for " + vname + " -> " +
           (now_allowed ? "ON" : "OFF"));
}

//---------------------------------------------------------
// Procedure: selectedVName()

string RedirectWaypoint::selectedVName() const
{
  if((m_controlled_team == "") || (m_sel_index == 0))
    return("");
  const vector<string>& vec = teamRoster(m_controlled_team);
  if(m_sel_index > vec.size())
    return("");
  return(vec[m_sel_index - 1]);
}

//---------------------------------------------------------
// Procedure: handleClick()
//   RDR_CLICK form: "x=<m>,y=<m>" (local grid). Ignored unless
//   click mode is on and a valid boat is selected.

bool RedirectWaypoint::handleClick(string click_str)
{
  m_clicks_received++;

  // A click acts only if the SELECTED team is currently controlled
  // (controlled == clickable). If no team selected or the selected
  // team is in autonomy, ignore the click.
  if(m_controlled_team == "") {
    m_last_warning = "Click ignored -- no team selected (press b or r)";
    debugLog(m_last_warning);
    return(false);
  }
  if(m_controlled_teams.count(m_controlled_team) == 0) {
    m_last_warning = "Click ignored -- " + m_controlled_team +
                     " not controlled (press w to control it)";
    debugLog(m_last_warning);
    return(false);
  }

  string xstr = tokStringParse(click_str, "x", ',', '=');
  string ystr = tokStringParse(click_str, "y", ',', '=');
  if((xstr == "") || (ystr == "")) {
    m_last_warning = "Malformed click: " + click_str;
    debugLog(m_last_warning);
    return(false);
  }

  string vname = selectedVName();
  if(vname == "") {
    m_last_warning = "Click ignored -- no valid boat selected "
                     "(team=" + m_controlled_team + " idx=" +
                     uintToString(m_sel_index) + ")";
    debugLog(m_last_warning);
    return(false);
  }

  double x = atof(xstr.c_str());
  double y = atof(ystr.c_str());
  dispatchWaypoint(x, y);
  return(true);
}

//---------------------------------------------------------
// Procedure: dispatchWaypoint()
//   Poke the per-vehicle command vars for the selected boat.
//   REDIR_ACTIVE activates waypt_redirect (pwt 150) which
//   outbids station_hold (pwt 140); on arrival the behavior
//   clears REDIR_ACTIVE and station_hold resumes, holding the
//   boat at the clicked point.

void RedirectWaypoint::dispatchWaypoint(double x, double y)
{
  string vname = selectedVName();
  if(vname == "")
    return;
  string up = toupper(vname);

  string update;
  update += "points=" + doubleToStringX(x,1) + "," + doubleToStringX(y,1);
  update += "#capture_radius=" + doubleToStringX(m_capture_radius,1);
  update += "#slip_radius="    + doubleToStringX(m_slip_radius,1);
  update += "#speed="          + doubleToStringX(m_speed,1);

  Notify(m_update_var + "_" + up, update);
  // Clear the arrival latch BEFORE re-activating: REDIR_ARRIVED is
  // set true on the vehicle by the waypt_redirect endflag and gates
  // constant_heading_redirect. Without this clear, that behavior
  // stays active through the new transit and bids a stale heading
  // (pwt 100) against waypt_redirect (pwt 150).
  Notify(m_arrived_var + "_" + up, "false");
  Notify(m_active_var + "_" + up, "true");

  m_waypoints_sent++;
  m_last_target  = vname;
  m_last_point   = "x=" + doubleToStringX(x,1) + ", y=" + doubleToStringX(y,1);
  m_last_warning = "";
  debugLog("Sent waypoint to " + vname + " (" + m_last_point + ")");
}

//---------------------------------------------------------
// Procedure: debugLog()

void RedirectWaypoint::debugLog(const string& line)
{
  if(!m_debug)
    return;
  string stamped = doubleToString(MOOSTime(), 2) + "  " + line;
  m_debug_buffer.push_back(stamped);
  while(m_debug_buffer.size() > m_debug_buffer_max)
    m_debug_buffer.pop_front();
}

//---------------------------------------------------------
// Procedure: buildReport()

bool RedirectWaypoint::buildReport()
{
  string sel_vname = selectedVName();
  if(sel_vname == "")
    sel_vname = "(none / out of range)";

  m_msgs << "Operator State"                                      << endl;
  {
    string ct;
    set<string>::iterator s;
    for(s=m_controlled_teams.begin(); s!=m_controlled_teams.end(); s++) {
      if(ct != "") ct += ", ";
      ct += *s;
    }
    m_msgs << "  Controlled teams:" << (ct==""?"(none)":ct)        << endl;
  }
  m_msgs << "  Click target:    "
         << (m_controlled_team==""?"(none)":m_controlled_team);
  if(m_controlled_team != "")
    m_msgs << (m_controlled_teams.count(m_controlled_team)
               ? "  [controlled]" : "  [not controlled -- press w]");
  m_msgs << endl;
  m_msgs << "  Boat index:      " << m_sel_index                  << endl;
  m_msgs << "  Selected boat:   " << sel_vname                    << endl;
  {
    string ua;
    set<string>::iterator s;
    for(s=m_untag_allowed.begin(); s!=m_untag_allowed.end(); s++) {
      if(ua != "") ua += ", ";
      ua += *s;
    }
    if(ua == "") ua = "(none)";
    m_msgs << "  Untag-allowed:   " << ua                         << endl;
  }
  m_msgs << endl;

  m_msgs << "Rosters"                                             << endl;
  m_msgs << "  blue: " << stringVectorToString(m_blue_vehicles, ',') << endl;
  m_msgs << "  red:  " << stringVectorToString(m_red_vehicles, ',')  << endl;
  m_msgs << endl;

  m_msgs << "Activity"                                            << endl;
  m_msgs << "  Clicks received: " << m_clicks_received            << endl;
  m_msgs << "  Waypoints sent:  " << m_waypoints_sent             << endl;
  m_msgs << "  Last target:     " << m_last_target                << endl;
  m_msgs << "  Last point:      " << m_last_point                 << endl;
  if(m_last_warning != "")
    m_msgs << "  NOTE:            " << m_last_warning             << endl;

  if(m_debug && !m_debug_buffer.empty()) {
    m_msgs << endl << "Debug (newest last)"                       << endl;
    list<string>::iterator q;
    for(q=m_debug_buffer.begin(); q!=m_debug_buffer.end(); q++)
      m_msgs << "  " << *q << endl;
  }
  return(true);
}

#--------------------------------------------------------
# The CMakeLists.txt for:                pRedirectWaypoint
# Author(s):                             Tyler Errico
#--------------------------------------------------------

SET(SRC
  RedirectWaypoint.cpp
  RedirectWaypoint_Info.cpp
  main.cpp
)

ADD_EXECUTABLE(pRedirectWaypoint ${SRC})

TARGET_LINK_LIBRARIES(pRedirectWaypoint
  ${MOOS_LIBRARIES}
  contacts
  geometry
  apputil
  mbutil
  m
  pthread)

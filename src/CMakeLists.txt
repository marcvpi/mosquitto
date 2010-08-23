include_directories(${mosquitto_SOURCE_DIR} ${mosquitto_SOURCE_DIR}/src
		${mosquitto_SOURCE_DIR}/lib)

add_executable(mosquitto 
	bridge.c
	conf.c
	context.c
	database.c
	logging.c
	../lib/memory_mosq.c ../lib/memory_mosq.h
	mosquitto.c
	mqtt3.h
	net.c
	../lib/net_mosq.c ../lib/net_mosq.h
	raw_send.c raw_send_client.c raw_send_server.c
	read_handle.c read_handle_client.c read_handle_server.c
	util.c
	../lib/util_mosq.c ../lib/util_mosq.h)

option(USE_LIBWRAP
	"Include tcp-wrappers support?" OFF)
option(INC_DB_UPGRADE
	"Include database upgrade support? (recommended)" ON)
option(INC_REGEX
	"Include topic regex matching support? (recommended)" ON)
option(INC_MEMTRACK
	"Include memory tracking support?" ON)

add_definitions (-DWITH_BROKER)
target_link_libraries(mosquitto sqlite3)
set_target_properties(mosquitto PROPERTIES
	VERSION ${VERSION})

install(TARGETS mosquitto RUNTIME DESTINATION ${BINDEST} LIBRARY DESTINATION ${LIBDEST})

if (UNIX)
	install(CODE "EXEC_PROGRAM(/sbin/ldconfig)")
endif (UNIX)

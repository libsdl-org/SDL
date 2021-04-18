include("${CMAKE_CURRENT_LIST_DIR}/SDL2Targets.cmake")


# cuts off the path at the last (back)slash
function(my_dirname FILEPATH RETVAL)
	string(FIND ${FILEPATH} "/" slashPos REVERSE)
	string(FIND ${FILEPATH} "\\" bsPos REVERSE) # TODO: untested, cmake always gave me forward slashes
	if(DEFINED slashPos)
		if( (DEFINED bsPos) AND (${bsPos} GREATER ${slashPos}) )
			set(slashPos, ${bsPos})
		endif()
	elseif(DEFINED bsPos)
		set(slashPos, ${bsPos})
	endif()
	if(DEFINED slashPos)
		string(SUBSTRING ${FILEPATH} 0 ${slashPos} dirname)
		set(${RETVAL} ${dirname} PARENT_SCOPE)
	endif()
endfunction()


if(WIN32)
	# this seems to work for both MSVC and MINGW+MSYS and with both SDL2Config/Target.cmake from vcpkg
	# and from building myself with cmake from latest git

	# ok, the headers are easy - but note that this adds both .../include and .../include/SDL2/
	# while the SDL2_INCLUDE_DIRS of sdl2-config.cmake only add ...include/SDL2/
	# But at least if building worked with sdl2-config.cmake it will also work with this.
	get_target_property(SDL2_INCLUDE_DIRS SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)

	# get the paths to the .lib files for both SDL2 and SDL2main
	# they could be in lots of different properties..
	get_target_property(sdl2implib SDL2::SDL2 IMPORTED_IMPLIB_RELEASE)
	get_target_property(sdl2implibdbg SDL2::SDL2 IMPORTED_IMPLIB_DEBUG)

	# TODO: for all this bla_RELEASE bla_DEBUG etc stuff, maybe only try the ones
	#       from get_target_property(available_configs SDL2::SDL2 IMPORTED_CONFIGURATIONS) ?
	#       OR otherwise at least try all of NOCONFIG, RELEASE, MINSIZEREL, RELWITHDEBINFO, DEBUG  (possibly in that order)?

	get_target_property(sdl2mainimplib SDL2::SDL2main IMPORTED_IMPLIB_RELEASE)
	if(NOT sdl2mainimplib)
		get_target_property(sdl2mainimplib SDL2::SDL2main IMPORTED_LOCATION_RELEASE)
	endif()
	get_target_property(sdl2mainimplibdbg SDL2::SDL2main IMPORTED_IMPLIB_DEBUG)
	if(NOT sdl2mainimplibdbg)
		get_target_property(sdl2mainimplibdbg SDL2::SDL2main IMPORTED_LOCATION_DEBUG)
	endif()
	
	if( sdl2implib AND sdl2mainimplib AND sdl2implibdbg AND sdl2mainimplibdbg )
		# we have both release and debug builds of SDL2 and SDL2main, so use this ugly
		# generator expression in SDL2_LIBRARIES to support both in MSVC, depending on build type configured there
		set(SDL2_LIBRARIES $<IF:$<CONFIG:Debug>,${sdl2mainimplibdbg},${sdl2mainimplib}>   $<IF:$<CONFIG:Debug>,${sdl2implibdbg},${sdl2implib}>)
	else()
		if(NOT sdl2implib) # try to get it from other properties
			foreach(prop IMPORTED_IMPLIB IMPORTED_IMPLIB_NOCONFIG IMPORTED_IMPLIB_DEBUG IMPORTED_LOCATION_RELEASE IMPORTED_LOCATION_DEBUG)
				get_target_property(sdl2implib SDL2::SDL2 ${prop})
				if(sdl2implib)
					message(STATUS "succeeded with ${prop} => ${sdl2implib}")
					break()
				endif()
				message(STATUS "no luck with ${prop}")
			endforeach()
		endif()
		if(NOT sdl2mainimplib)
			foreach(prop IMPORTED_IMPLIB IMPORTED_IMPLIB_NOCONFIG IMPORTED_IMPLIB_DEBUG IMPORTED_LOCATION_RELEASE IMPORTED_LOCATION_DEBUG)
				get_target_property(sdl2mainimplib SDL2::SDL2main ${prop})
				if(sdl2mainimplib)
					message(STATUS "succeeded with ${prop} => ${sdl2mainimplib}")
					break()
				endif()
				message(STATUS "no luck with ${prop}")
			endforeach()
		endif()
		
		if( sdl2implib AND sdl2mainimplib )
				set(SDL2_LIBRARIES ${sdl2mainimplib}  ${sdl2implib})
		else()
				message(FATAL_ERROR, "SDL2::SDL2 and/or SDL2::SDL2main don't seem to contain any kind of IMPORTED_IMPLIB")
		endif()
	endif()

	# NOTE: SDL2_LIBRARIES now looks like "c:/path/to/SDL2main.lib;c:/path/to/SDL2.lib"
	#       which is different to what it looks like when coming from sdl2-config.cmake
	#       (there it's more like "-L${SDL2_LIBDIR} -lSDL2main -lSDL2" - and also -lmingw32 and -mwindows)
	#       This seems to work with both MSVC and MinGW though, while the other only worked with MinGW

	my_dirname(${sdl2implib} SDL2_LIBDIR)

	my_dirname(${SDL2_LIBDIR} SDL2_EXEC_PREFIX) # the exec prefix is one level up from lib/ - TODO: really, always? at least on Linux there's /usr/lib/x86_64-bla-blub/libSDL2-asdf.so.0 ..
	set(SDL2_PREFIX ${SDL2_PREFIX}) # TODO: could this be somewhere else? parent dir of include or sth?

elseif() # not windows
	# TODO: about the same but don't have to look at *_IMPLIB* as that's windows specific, it should be in IMPORTED_LOCATION*
	# TODO: make SDL2_LIBRARIES look more like the original (instead of path to libSDL2.so)?
	# TODO: anything special about macOS? I can't test that, I have no Mac
endif()

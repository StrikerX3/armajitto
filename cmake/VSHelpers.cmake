# Add Visual Studio filters to better organize the code
function(vs_set_filters)
	cmake_parse_arguments(VS_SET_FILTERS "" "TARGET;BASE_DIR" "" ${ARGN})
	if(MSVC)
		get_target_property(VS_TARGET_SOURCES ${VS_SET_FILTERS_TARGET} SOURCES)
		string(REPLACE "\\" "/" VS_SET_FILTERS_BASE_DIR "${VS_SET_FILTERS_BASE_DIR}")
		if(NOT "${VS_SET_FILTERS_BASE_DIR}" MATCHES "/$")
			string(APPEND VS_SET_FILTERS_BASE_DIR "/")
		endif()

	    # Organize files into folders (filters) mirroring the file system structure
		foreach(FILE IN ITEMS ${VS_TARGET_SOURCES}) 
			# Get file directory
			get_filename_component(FILE_DIR "${FILE}" DIRECTORY)

		    # Normalize path separators
		    string(REPLACE "\\" "/" FILE_DIR "${FILE_DIR}")

		    # Erase base directory from the beginning of the path
		    string(REGEX REPLACE "^${VS_SET_FILTERS_BASE_DIR}" "" FILE_DIR "${FILE_DIR}")

	    	# Put files into folders mirroring the file system structure
			source_group("${FILE_DIR}" FILES "${FILE}")
		endforeach()
	endif()
endfunction()

# Make the Debug and RelWithDebInfo targets use Program Database for Edit and Continue for easier debugging
function(vs_use_edit_and_continue)
	if(MSVC)
		string(REPLACE "/Zi" "/ZI" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
		string(REPLACE "/Zi" "/ZI" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
		set(CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG} PARENT_SCOPE)
		set(CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} PARENT_SCOPE)
	endif()
endfunction()

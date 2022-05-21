
if (UNIX)
    set(PYTHON_COMMAND "/usr/bin/python3")
else()
    set(PYTHON_COMMAND "python3")
endif()

# Regular Local Testing
add_test(NAME E2E COMMAND ${PYTHON_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/e2e-runner.py $<TARGET_FILE:adept> WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

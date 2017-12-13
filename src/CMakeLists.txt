###########################
# Main exe
###########################
file(GLOB main_SRC "*.cpp")

add_executable(dq_thread_safe_conn ${main_SRC})
set_property(TARGET dq_thread_safe_conn PROPERTY CXX_STANDARD 11)

# ~~~
# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ~~~

#
# cpp_bigquery_odbc_set_target_name : formats the prefix and path as a target
#
# The formatted target name looks like "<prefix>_<basename>" where <basename> is
# computed from the path.
#
# * target the name of the variable to be set in the parent scope to hold the
#   target name.
# * prefix a unique string to prepend to the target name. Usually this should be
#   a string indicating the type of target, such as "integration_tests" or
#   "client_interface_unit_tests".
# * path is the filename that should be used for the target.
#
function (cpp_bigquery_odbc_set_target_name target prefix path)
    string(REPLACE "/" "_" basename "${path}")
    string(REPLACE ".cc" "" basename "${basename}")
    set("${target}"
        "${prefix}_${basename}"
        PARENT_SCOPE)
endfunction ()

#
# cpp_bigquery_odbc_add_executable : adds an executable w/ the given source and
# prefix name
#
# Computes the target name using `cpp_bigquery_odbc_set_target_name` (see
# above), then adds an executable with a few common properties. Sets the
# `target` in the caller's scope to the name of the computed target name.
#
# * target the name of the variable to be set in the parent scope to hold the
#   target name.
# * prefix a unique string to prepend to the target name. Usually this should be
#   a string indicating the product, such as "pubsub" or "storage".
# * path is the filename that should be used for the target.
#
function (cpp_bigquery_odbc_add_executable target prefix source)
    cpp_bigquery_odbc_set_target_name(target_name "${prefix}" "${source}")
    add_executable("${target_name}" "${source}")
    set("${target}"
        "${target_name}"
        PARENT_SCOPE)
endfunction ()

#pragma once

#include <common/SingleInstance.hpp>
#include <common/AppId.hpp>

namespace txui {

using tinexus::common::get_canonical_app_id;
using tinexus::common::is_single_instance_app;
using SingleInstance = tinexus::common::SingleInstance;

} // namespace txui

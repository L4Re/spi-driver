/*
 * Copyright (C) 2024 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt philipp.eppelt@kernkonzept.com
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "spi_controller_if.h"

namespace Spi_server {
void start_server(Controller_if *ctrl);
}

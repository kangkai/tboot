/*
 * Copyright (C) 2012 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Device and platform related functions.
 */

#include "platform.h"

/*
 * Get the current running platform, e.g. BlackBay or RedRidge etc.
 *
 * FIXME: Currently the implementation is getting that through kernel cmdline,
 * then pass and save that in a global structure platform_info, this is a
 * little bit ugly. Later we'll find a better solution.
 */

/*
 * Kernel cmdline for tboot should have one of this:
 * tboot.platform=blackbay
 * tboot.platform=redridge
 * tboot.platform=redhookbay
 */
struct platform_info plat_info;

int set_current_platform(PLATFORM_T p)
{
	if (p < PLATFORM_UNKNOWN || p > PLATFORM_MAX)
		return -1;

	plat_info.platform = p;
	return 0;
}

PLATFORM_T get_current_platform(void)
{
	return plat_info.platform;
}

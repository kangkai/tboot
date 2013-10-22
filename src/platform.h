/*
 * Copyright 2012 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

typedef enum {
	PLATFORM_UNKNOWN = 0,
	PLATFORM_BLACKBAY = 1,
	PLATFORM_REDRIDGE = 2,
	PLATFORM_REDHOOKBAY = 3,
	PLATFORM_MAX = 3

} PLATFORM_T;

struct platform_info {
	PLATFORM_T platform;
};

int set_current_platform(PLATFORM_T p);
PLATFORM_T get_current_platform(void);

#endif

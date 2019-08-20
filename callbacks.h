/*************************************************************************/
/*  callbacks.h                                                          */
/*************************************************************************/
/*                                                                       */
/*       FMOD Studio module and bindings for the Godot game engine       */
/*                                                                       */
/*************************************************************************/
/* Copyright (c) 2019 Alex Fonseka                                       */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#pragma once

#include "core/dictionary.h"
#include "core/map.h"

#include "api/studio/inc/fmod_studio.hpp"

namespace Callbacks {

	struct CallbackInfo {
		Dictionary markerCallbackInfo;
		Dictionary beatCallbackInfo;
		Dictionary soundCallbackInfo;

		bool markerSignalEmitted	= true;
		bool beatSignalEmitted		= true;
		bool soundSignalEmitted		= true;
	};

	extern Mutex *mut;

	FMOD_RESULT F_CALLBACK eventCallback(FMOD_STUDIO_EVENT_CALLBACK_TYPE type, FMOD_STUDIO_EVENTINSTANCE *event, void *parameters);

} // namespace Callbacks

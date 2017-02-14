/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "cg_local.h"

#include "PrimaryIcon.h"

#define _Class _PrimaryIcon

#pragma mark - View

/**
 * @see View::respondToEvent(View *, const SDL_Event *)
 */
static void respondToEvent(View *self, const SDL_Event *event) {

	super(View, self, respondToEvent, event);

	((Control *) self)->bevel = ControlBevelTypeNone;
}

#pragma mark - PrimaryIcon

/**
 * @fn PrimaryIcon *PrimaryIcon::init(PrimaryIcon *self)
 *
 * @memberof PrimaryIcon
 */
static PrimaryIcon *initWithFrame(PrimaryIcon *self, const SDL_Rect *frame, const char *icon) {

	self = (PrimaryIcon *) super(Button, self, initWithFrame, frame, ControlStyleCustom);
	if (self) {
		self->imageView = $(alloc(ImageView), initWithFrame, frame);
		assert(self->imageView);

		$(self->imageView, setImage, NULL);

		SDL_Surface *surface;

		if (cgi.LoadSurface(icon, &surface)) {

			$(self->imageView, setImageWithSurface, surface);
			SDL_FreeSurface(surface);
		}

		self->imageView->view.alignment = ViewAlignmentMiddleCenter;
		self->imageView->view.autoresizingMask = ViewAutoresizingFill;

		$((View *) self, addSubview, (View *) self->imageView);

		self->button.control.view.backgroundColor = Colors.Clear;

		self->button.control.bevel = ControlBevelTypeNone;

		self->button.control.view.borderWidth = 1;
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->def->interface)->respondToEvent = respondToEvent;

	((PrimaryIconInterface *) clazz->def->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *PrimaryIcon::_PrimaryIcon(void)
 * @memberof PrimaryIcon
 */
Class *_PrimaryIcon(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "PrimaryIcon";
		clazz.superclass = _Button();
		clazz.instanceSize = sizeof(PrimaryIcon);
		clazz.interfaceOffset = offsetof(PrimaryIcon, interface);
		clazz.interfaceSize = sizeof(PrimaryIconInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class

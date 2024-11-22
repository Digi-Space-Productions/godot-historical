/*************************************************************************/
/*  path_2d_editor_plugin.h                                              */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                    http://www.godotengine.org                         */
/*************************************************************************/
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                 */
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
#ifndef PATH_2D_EDITOR_PLUGIN_H
#define PATH_2D_EDITOR_PLUGIN_H

#include "tools/editor/editor_plugin.h"
#include "tools/editor/editor_node.h"
#include "scene/2d/path_2d.h"
#include "scene/gui/tool_button.h"
#include "scene/gui/button_group.h"

/**
	@author Juan Linietsky <reduzio@gmail.com>
*/
class CanvasItemEditor;

class Path2DEditor : public ButtonGroup {

	OBJ_TYPE(Path2DEditor, ButtonGroup );

	UndoRedo *undo_redo;

	CanvasItemEditor *canvas_item_editor;
	EditorNode *editor;
	Panel *panel;
	Path2D *node;

	enum Action {

		ACTION_NONE,
		ACTION_MOVING_POINT,
		ACTION_MOVING_IN,
		ACTION_MOVING_OUT,
	};


	Action action;
	int action_point;
	Point2 moving_from;
	Point2 moving_screen_from;


	void _canvas_draw();

protected:
	void _notification(int p_what);
	void _node_removed(Node *p_node);
	static void _bind_methods();
public:

	Vector2 snap_point(const Vector2& p_point) const;
	bool forward_input_event(const InputEvent& p_event);
	void edit(Node *p_collision_polygon);
	Path2DEditor(EditorNode *p_editor);
};

class Path2DEditorPlugin : public EditorPlugin {

	OBJ_TYPE( Path2DEditorPlugin, EditorPlugin );

	Path2DEditor *collision_polygon_editor;
	EditorNode *editor;

public:

	virtual bool forward_input_event(const InputEvent& p_event) { return collision_polygon_editor->forward_input_event(p_event); }

	virtual String get_name() const { return "Path2D"; }
	bool has_main_screen() const { return false; }
	virtual void edit(Object *p_node);
	virtual bool handles(Object *p_node) const;
	virtual void make_visible(bool p_visible);

	Path2DEditorPlugin(EditorNode *p_node);
	~Path2DEditorPlugin();

};



#endif // PATH_2D_EDITOR_PLUGIN_H

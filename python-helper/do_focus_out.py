from gi.repository import IBus, Gdk

class MyIMEEngine(IBus.Engine):
    def __init__(self, **kwargs):
        super(MyIMEEngine, self).__init__(**kwargs)
        self.preedit_string = ""
        self.cursor_pos = 0
        
        # Initialize LookupTable: 10 items per page, starting at 0, visible, circular
        self.lookup_table = IBus.LookupTable.new(10, 0, True, True)
        self.lookup_table.set_orientation(IBus.Orientation.VERTICAL)

    def do_process_key_event(self, keyval, keycode, state):
        # 1. Handle Candidate Selection (if list is visible)
        if self.lookup_table.get_number_of_candidates() > 0:
            if keyval == IBus.KEY_Down:
                if self.lookup_table.cursor_down():
                    self.update_lookup_table(self.lookup_table, True)
                return True
            elif keyval == IBus.KEY_Up:
                if self.lookup_table.cursor_up():
                    self.update_lookup_table(self.lookup_table, True)
                return True
            elif keyval in (IBus.KEY_Return, IBus.KEY_space):
                self.commit_selected_candidate()
                return True

        # 2. Handle Text Input
        if not (state & (Gdk.ModifierType.CONTROL_MASK | Gdk.ModifierType.MOD1_MASK)):
            if 32 <= keyval <= 126:  # Simple ASCII range
                char = chr(keyval)
                self.preedit_string += char
                self.cursor_pos += 1
                self.refresh_ui()
                return True
            
        # 3. Handle Backspace
        if keyval == IBus.KEY_BackSpace and self.preedit_string:
            self.preedit_string = self.preedit_string[:-1]
            self.cursor_pos = max(0, self.cursor_pos - 1)
            self.refresh_ui()
            return True

        return False

    def refresh_ui(self):
        """Updates both the pre-edit text and the candidate list."""
        if not self.preedit_string:
            self.reset_state()
            return

        # Create styled pre-edit text
        text = IBus.Text.new_from_string(self.preedit_string)
        # Add blue background to the character at the current cursor position
        text.append_attribute(IBus.AttributeType.BACKGROUND, 0x0000FF, 
                              self.cursor_pos - 1, self.cursor_pos)
        # Add underline to entire string
        text.append_attribute(IBus.AttributeType.UNDERLINE, 
                              IBus.AttrUnderline.SINGLE, 0, len(self.preedit_string))
        
        self.update_preedit_text(text, self.cursor_pos, True)

        # Update candidates (Example: just repeating the string 3 times)
        candidates = [self.preedit_string, self.preedit_string * 2, "example"]
        self.lookup_table.clear()
        for c in candidates:
            self.lookup_table.append_candidate(IBus.Text.new_from_string(c))
        
        self.update_lookup_table(self.lookup_table, True)

    def commit_selected_candidate(self):
        idx = self.lookup_table.get_cursor_pos()
        candidate = self.lookup_table.get_candidate(idx)
        self.commit_text(candidate)
        self.reset_state()

    def do_focus_out(self):
        """Ensure text is committed when user clicks away."""
        if self.preedit_string:
            self.commit_text(IBus.Text.new_from_string(self.preedit_string))
        self.reset_state()

    def reset_state(self):
        """Cleans up the UI and internal buffers."""
        self.preedit_string = ""
        self.cursor_pos = 0
        self.lookup_table.clear()
        self.update_preedit_text(IBus.Text.new_from_string(""), 0, False)
        self.update_lookup_table(self.lookup_table, False)

# 🔑 Key Integration PointsAttribute Application: 
# The refresh_ui method applies a background color attribute specifically to the character just typed (or moved to), 
# giving the user visual focus.Safety via do_focus_out: By overriding this, 
# you prevent the common bug where user input "vanishes" when switching browser tabs or windows.
# The Reset Loop: Both commit_selected_candidate and 
# do_focus_out call reset_state to ensure the candidate window doesn't "ghost" (stay visible) after the interaction ends.To finalize this into a production-ready tool, 
# should I show you how to load a JSON dictionary to replace the dummy candidates in the refresh_ui method?


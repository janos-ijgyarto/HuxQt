HUX USER GUIDE
==============

1. Main window
	- Provides a browser for scenario contents, and a display that shows a preview of the terminal contents
	- To open a scenario for viewing/editing, navigate to a split folder (e.g created by Atque). The app will load all valid terminal script files.
	- Clicking on a tree item will navigate to the first available screen node in the item's subtree.
	- The selected screen is shown in the preview display. Use the buttons to step back and forth between the other screens.
	- Screens are grouped according to the Aleph One terminal logic (i.e Unfinished/Finished).
	- Additional metadata for the currently selected terminal can be found under the scenario browser.
	- SAVING SCENARIO SCRIPTS CURRENTLY DISABLED
	- UNDO/REDO NOT IMPLEMENTED
	- ADDING/MOVING/REMOVING SCENARIO ITEMS NOT IMPLEMENTED
	- For testing purposes, the edited scenario can be exported to an empty folder
	
2. Terminal editing
	- Double-clicking a terminal item will bring up the Terminal edit dialog
	- The terminal metadata (teleport, etc.) can be edited using the controls
	- Inputs must be valid, and the terminal ID must not conflict with the others in the same level, or the edits will be rejected.
	- When done editing, click OK to save the changes.

3. Screen editing
	- Double-clicking a screen item in the scenario browser will bring up the Screen Editor
	- Each screen is given its own tab in the editor, along with controls to edit the contents
	- The display shows the preview of the currently selected screen (automatically updated once edits are made)
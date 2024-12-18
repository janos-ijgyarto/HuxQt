# Hux
Hux is a WYSIAWYG ("What You See Is (Almost) What You Get") terminal script editor for [Aleph One](https://alephone.lhowon.org/), developed using [Qt](https://www.qt.io/). The GUI allows users to create, edit, and preview terminals in any AO-compatible scenario, with controls provided for image and text alignment, formatting, etc. 

All necessary operations can be done through the tool, minimizing the risk of errors caused by incorrect syntax, and reducing the time cost of creating/editing terminals.

The app was developed using Qt version 6.8.0.

_NOTE: Hux provides a close, but not 1:1 representation of Aleph One's terminal rendering. All scripting content is preserved, but there can be small differences in the displayed result (e.g line wrapping)_

## Building from source

Hux uses [CMake](https://cmake.org/) as its platform-independent build system. The minimum required version is 3.16.

For convenience, the project also comes with a [CMakePresets.json](https://learn.microsoft.com/en-us/cpp/build/cmake-presets-vs) containing some basic build configurations for Windows and Linux. These can be further adjusted via a CMakeUserPresets.json file for the user's local environment.

### Building on Windows

The simplest option is to build using Visual Studio 2022 or Visual Studio Code with the relevant extensions, as both are able to use CMake presets.

*NOTE: the CMake command `find_package` currently does not work correctly with Qt on Windows, so you have to set the `CMAKE_PREFIX_PATH` variable to where Qt6 is installed.*

### Building on Linux

Set the requirements for Linux/X11 as per the [Qt documentation](https://doc.qt.io/qt-6/linux.html), then install the "Qt6 base" package (e.g `sudo apt install qt6-base-dev`), which will have the necessary Qt features. Install CMake, then configure and build.

*NOTE: make sure to set an output directory (i.e `CMAKE_RUNTIME_OUTPUT_DIRECTORY`), otherwise the application name may collide with one of the folders created by CMake during the build.*

### Building on Mac

TODO

## Getting started

Once you open Hux, you must first either [import](#importing-scenarios) a scenario from a split folder, or [load](#loading-scenarios) a Hux scenario file.

### Importing scenarios

Hux can make use of split folders generated by [Atque](https://sourceforge.net/projects/igniferroque/) to load the terminal data.

To import a scenario from a split folder, simply click _File -> Import Scenario Scripts_ and select the root directory of a split scenario generated by Atque. The scenario browser will display all the levels that have valid terminal script files.

 When you start work on a new scenario, or want to edit an existing scenario that has no Hux-specific data, you must create a split folder and import it, after which you can [add levels](#editing-scenarios) that require terminal data.

*NOTE: make sure the split folder also has a valid "Resources" folder!*

### Loading scenarios

Hux uses a custom file to save/load terminal scripts (using JSON). 
The file is read and written by the application, no manual editing is necessary (except when merging changes, e.g via a diff tool).


To load a Hux scenario file, click _File -> Load Scenario_ and select a valid scenario JSON file.

The custom file is recommended for developers who work on terminals, since it allows them to store additional metadata and can more easily merge changes between multiple users.

*NOTE: Hux expects the scenario file to be in the same directory as the "Resources" folder for the same scenario, otherwise it cannot load the images referenced in the scripts!*

### Main window

This is the main window that is first displayed when the app is loaded. 

The left side of the window shows the [Scenario Browser](#scenario-browser) and an information table for the currently selected terminal. 

The right side shows a [Screen Browser](#screen-browser) for the selected terminal, a table containing information about the currently selected screen, and a terminal preview display.

#### Scenario Browser

Each level in the scenario is represented by a folder in the list view. 

Double-clicking a level will list the terminals contained within said level. 

Clicking on a terminal will show its contents in the [Screen Browser](#screen-browser). To return to the main scenario view, use the "up" button above the *Scenario Browser*.

#### Screen Browser

Every terminal's screens are split into the "UNFINISHED" and "FINISHED" sections. 

Clicking on any screen item will show its information and render its preview on the display. 

The buttons under the display can also be used to navigate between the selected terminal's screens.

### Editing scenarios

The [Scenario Browser](#scenario-browser) lets you edit the scenario contents, including both levels and their terminals.

- Add and remove is done via the buttons under the [Scenario Browser](#scenario-browser) view.
- Levels can be edited via right-clicking and selecting "Edit Level". This opens the [Level Editor](#level-editor) dialog.
- You can use drag & drop to reorder levels and terminals. Changing the level order does not affect the export, it is only for user convenience.
  - *NOTE: changing terminal order will change the terminal IDs in the exported script. This can invalidate references in the map data!*
- Double-clicking a level opens it and displays a list of its terminals.
- Terminals can be copy & pasted within and between levels. To copy selected terminals, right-click and select "Copy", then right-click again in the desired location and select "Paste".
- Double-clicking a terminal in the [Scenario Browser](#scenario-browser) opens a [Terminal Editor](#terminal-editor) window, which allows users to edit the terminal contents.

### Level Editor

This dialog allows you to modify the level attributes. You can edit the level name, the script file name, and the level folder name.

The level name is for user convenience only. Its display name in-game needs to be set up in the relevant scripts outside of Hux.

When adding a new level or reorganizing the scenario, you must be provide the correct folder and script file names. This allows Hux to overwrite the appropriate files when exporting the scenario.

*NOTE: the current implementation excludes characters from folder and file names that are not allowed in the Windows file system!*

### Terminal Editor

When you double-click a terminal in the [Scenario Browser](#scenario-browser), it opens a [Terminal Editor](#terminal-editor) window for this terminal.

The left side of the window shows controls for editing terminal attributes (i.e teleport info) and the [Screen Browser](#screen-browser), which can be used to view and modify the screens within the terminal. 

The right side contains an editor view where the screen contents and metadata can be edited, along with a preview display for the currently edited screen.

- You can give terminals a custom name to identify them more easily. This data is Hux-specific, it does not get exported with the terminal script files.
- The [Screen Browser](#screen-browser) contains two folders, corresponding to the "UNFINISHED" and "FINISHED" screen groups. Double-click a folder to view and edit its contents.
- Editing the screen list is similar to editing levels in the [Scenario Browser](#scenario-browser):
  - Add or remove screens using the buttons below the browser.
  - Move screens using drag & drop.
  - Copy & paste selected screens by right-clicking and selecting the appropriate action.
- To edit a screen, select it in the [Screen Browser](#screen-browser). This will update the screen editor view and the preview display. Switching screens will save changes to the last selected screen.
- Once you are done editing, you can close the window using "OK". This will apply the changes to the terminal in the [Scenario Browser](#scenario-browser).

### Editing screens

When a screen is selected in the [Screen Browser](#screen-browser), the editor view will be updated with its contents, and the preview for the screen will be shown in the display. 

Any changes made to the screen contents will automatically update the preview.

The page also shows line numbers to help keep track of how close the text content is to the line limit.

If a screen's text content exceeds the line limit for a single page, the preview display will show a page counter. The counter's value corresponds to the (estimated) number of pages into which the engine will break up the text.

- Previews for the page breaking is currently not yet implemented. It's recommended that you do not let the text exceed the line limit, as this allows for less precise control over the text layout.
- For more details on the syntax and terminal scripting logic, consult the Marathon Infinity manual.

### Saving scenarios

If you have unsaved changes, you can save the scenario file using _File->Save Scenario_. This will save the terminal scripting data to the Hux-specific JSON file.

When saving for the first time, you will be prompted for a scenario name and location. Subsequent saves will overwrite this file. To save the scenario to a different location, you can use _File->Save Scenario As_.

*NOTE: editor-specific data (e.g terminal names) are only saved in the JSON file. This data is lost if the user tries to reload a scenario from the exported .txt files!*

The user can also be prompted to save when exiting (this will skip the above steps).

*NOTE: the application expects the scenario file to be in the same folder as the Resources folder for the scenario, otherwise it cannot access the referenced images!*

### Exporting scenarios

In order to test the terminal changes in Aleph One, the terminal scripts have to be exported to a split folder, then merged using Atque.

To export a scenario, click _File -> Export Scenario Scripts_, which opens the export dialog. This will list all the levels and allows the user to preview the script contents. Clicking a level in the list will display the generated Aleph One terminal script.

Clicking OK will prompt the user to select a destination folder. This can be an existing split folder, in which case Hux will overwrite any existing terminal scripts. If no matching script is found for a level, Hux will create a new folder and script file.

*NOTE: make sure to adjust the level attributes so Hux overwrites the correct files in the correct folders!*

## License

See [LICENSE](https://github.com/janos-ijgyarto/HuxQt/blob/master/LICENSE) file.

## Useful links

- TODO: a link to the relevant manual for AO terminal scripting (e.g the Infinity manual)
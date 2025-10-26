# SuiteSpot
SuiteSpot is a BakkesMod plugin that allows users to automatically requeue or join a map of their choice.  
This plugin was heavily inspired by existing plugins (Instant Suite and Auto Leave).

## Table of Contents
- [Features](#features)
- [Prerequisite](#prerequisite)
- [Set-up](#setup)

# Features
This plugin lets you join any of the following:
- [Freeplay_Map](#freeplay_map)
- [Training_Map](#training_map)
- [Workshop_Map](#workshop_map)
- [Disclaimer](#disclaimer)

### Freeplay_Map
By using a combo box, users can choose the map they want to jump into after a match/game from the updated map list.  
It's as simple as choosing the map you want and making sure the SuiteSpot checkbox is ticked!

### Training_Map
Unlike existing plugins, SuiteSpot allows you to join a training map using a training map code.  
To join a training map, enter the map code and a name for the map. This adds it to your training map list, which you can access through the combo box.  
Simply select the map of your choice and you can load into it after a match.

### Workshop_Map
SuiteSpot lets you automatically join workshop maps after a match, provided they are already installed on your system.  
The plugin automates the **joining** process, while downloading and managing the maps is handled separately (see [Prerequisite](#prerequisite)).

# Prerequisite
You first need [BakkesMod](https://www.bakkesmod.com/) for this to work  
To use the **Workshop_Map** feature, you must install the [Workshop Map Loader & Downloader](https://bakkesplugins.com/plugins/view/223) plugin.  

- Use the Workshop Map Loader to **download** maps.  
- Ensure the maps are stored in the correct directory:  
C:\Program Files\Epic Games\rocketleague\TAGame\CookedPCConsole\mods  
- Once maps are downloaded and in the proper location, SuiteSpot can load them automatically after matches.

# Setup
1. First open BakkesMod using the F2 key, then locate the plugins tab  
![](images/rl1.png)  

2. Then locate the SuiteSpot plugin and select it  
![](images/rl2.png) 

3. Enable the plugin by clicking the checkbox, you can also choose to requeue by clicking the appropriate checkbox  
![](images/rl3.png) 

4. If you want to jump into a freeplay map, click the appropriate radio button and use the combo box to select the desired map  
![](images/rl4.png) 

5. If you wnat to jump into a training map, click the appropriate radio button and add a training map to your list.
Use the textboxes to add the map's code and name. Click the Add Training Map button to add.
![](images/rl5.png)  
You can now access that map in the combo box, allowing you to load in after a match  
![](images/rl6.png)  

6. If you want to jump into a workshop map, click the appropriate radio button and the combo box should populate with the maps that you have already downloaded
![](images/rl7.png)

## Disclaimer
This is my first plugin, so if you have any advice or recomendations feel free to contact me or clone by repo and code your own changes!  
Thanks! :)
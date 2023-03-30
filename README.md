<p align="center">
  <img width="500" height="100%" src="./docs/img/logo-text-colored-new.png">
</p>

<p align="center">
  <a href="https://opengoal.dev/docs/intro" rel="nofollow"><img src="https://img.shields.io/badge/Documentation-Here-informational" alt="Documentation Badge" style="max-width:100%;"></a>
  <a target="_blank" rel="noopener noreferrer" href="https://github.com/open-goal/jak-project/actions/workflows/build-matrix.yaml"><img src="https://github.com/open-goal/jak-project/actions/workflows/build-matrix.yaml/badge.svg" alt="Linux and Windows Build" style="max-width:100%;"></a>
  <a href="https://www.codacy.com/gh/open-goal/jak-project/dashboard?utm_source=github.com&utm_medium=referral&utm_content=open-goal/jak-project&utm_campaign=Badge_Coverage" rel="nofollow"><img src="https://app.codacy.com/project/badge/Coverage/29316d04a1644aa390c33be07289f3f5" alt="Codacy Badge" style="max-width:100%;"></a>
  <a href="https://www.codacy.com/gh/open-goal/jak-project/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=open-goal/jak-project&amp;utm_campaign=Badge_Grade" rel="nofollow"><img src="https://app.codacy.com/project/badge/Grade/29316d04a1644aa390c33be07289f3f5" alt="Codacy Badge" style="max-width:100%;"></a>
  <a href="https://discord.gg/VZbXMHXzWv"><img src="https://img.shields.io/discord/756287461377703987" alt="Discord"></a>
  <a href="https://makeapullrequest.com"><img src="https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square" alt=PRs Welcome></a>
</p>

# BEFORE YOU BEGIN

Please note this is not the main OpenGOAL repository! I'm currently working on creating a TAS (Tool Assisted Speedrun) for Jak and Daxter here, and if that's what you're looking for you've come to the right place.

If however you're looking for OpenGOAL itself, please start either on the main OpenGOAL site (https://opengoal.dev/), or on the main GitHub page (https://github.com/open-goal/jak-project).

From this point on I'm assuming you're already using OpenGOAL and are interested in contributing to the TAS!

I'm planning to work mostly in the `TAS` channel in the Jak Speedruns discord (https://discord.com/channels/83031186590400512/280432914829934592) at the moment, just because it's so appropriately titled, but that may end up changing more to the OpenGOAL discord in the future. Have a look in there for some example videos and messages I've added!

# Development environment setup

At the moment I'm not building separate releases for these builds, so you will need to set up your development environment using this repo and this branch. The instructions for setting up a dev environment can be found in the main OpenGOAL GitHub README: (https://github.com/open-goal/jak-project#setting-up-a-development-environment),

Once this has been developed a little further I'd like to make this either a mod or configuration option for the launcher, however it's still early days. In the mean time will require a little technical experience though unfortunately, so if you're not ready to take the plunge yet check back in on the progress later either here, or in the OpenGOAL or Jak Speedruns discords (https://discord.gg/VZbXMHXzWv) and (https://discord.gg/W9dJ5EePde).

If you are though, or have already set up the OpenGOAL development environment (and therefore will just need to run a separate clone with this repo + branch instead), then you should be good to go.

# Running the TAS

Once you have OpenGOAL running with `task boot-game` you're good to go, and should just be able to press `L3` to start it. While the TAS is running, you can press `Triangle` at any time to stop it (though you may need to hold it down for a little if you've slowed down the fps!)

If this is the first thing you do though, you'll see an error like:

`Failed to open inputs. Make sure to create a main.jaktas file in "jak-project/tas/jak1/" to get started! You can just copy one of the existing .jaktas files to test it.`

This is because the branch at the moment is only set up to look for 1 file to run. The expectation is that you'll either edit this `jak-project/tas/jak1/main.jaktas` file directly for testing, or `import` another file in `main.jaktas` to work from.

As an example, I'm currently using this as the content of my `main.jaktas` file while I'm testing:

```
# Quit to the title/logo screen and start a new game
import=util/quit-to-menu
import=util/create-new-game
import=no-lts-geyser-rock
```

This is almost an exact copy of `jak-project/tas/jak1/no-lts.jaktas`, but I've also included the `quit-to-menu` script at the start (making it easier to repeatedly test runs). You can also add additional instructions like `commands,frame-rate=999` at the top to run the TAS as fast as possible (you could go higher than 999 fps if you wanted, but good luck finding a machine that could run even that). More on these instructions below.

# TAS commands

I've been trying to keep examples of the current command set in the files as I've been working, but this will hopefully serve as a useful reference for these commands as well. I'll start with the basic structure of these files and then some of the individual lines within them.

## The JakTAS files

This file type is really just a `.txt` file in disguise, there's nothing special about it. The only reason I went with a separate extension is so that it would be a bit easier to build nice tooling for these files in the future (like syntax highlighting or error warnings). Basically as long as you have a text editor, you're good to go.

The parser will run through anything either in the `main.jaktas` file or in anything it points to (using the `import` keyword), but it will ignore any empty lines or lines prefixed with `#` (which are treated as comments). If you enter anything particularly wrong though, like mispelled or wrongly capitlised commands, it may crash without warning. Just try commenting out lines until you find the cause in this case, I'll add some nicer error explanations in the future!

I have put in a quick function to strip out whitespace so that `30 , Square` should be equivalent to `30,Square` for example, but try to remove all whitespace while you're working for the most reliable experience.

## Comments

These are mentioned above, but just for clarity: add a `#` to the start of any line to hide it from the parser. I've configured my `VSCode` to treat these as `diff` files for easy line comments and highlighting of just comments if that helps.

Additionally empty lines will be ignored.

## Import

The `import` commands are quite straight forward - just include a file name relative to the `jak-project/tas/` directory without the `.jaktas` extension and it should load it. Look at `no-lts.jaktas` as an example:

```
import=util/create-new-game
import=no-lts-geyser-rock
```

This will `import` the `jak-project/tas/util/create-new-game.jaktas` script (which creates a new game from the title/logo screen) and then run the `jak-project/tas/util/no-lts-geyser-rock.jaktas` script (which runs through Geyser Rock using `No LTS` strats).

You should in theory be able to import as many files as you like (including importing files within other files) as long as you're within your system limits, but don't go crazy testing that theory.

## Commands

The next type of lines are lines for configuring your environment. These will run at the point in time they're added, but aren't necessarily frame dependent. The commands that you can use are written below, and I'll include one example here to show that these can be chained:

`commands,frame-rate=999,marker=run-start`

### frame-rate

While you're testing it might be convenient to either slow down or speed up the running frame rate at certain points in time, so you can skip sections you're confident in and highlight ones that you're working on. The way to do this is with the `frame-rate` command, with some examples below:

```
commands,frame-rate=60
commands,frame-rate=999
commands,frame-rate=1
```

These will change your OpenGOAL instance to run at `60`, `999`, and `1` FPS respectively. These can be entered in almost any line to change the FPS when that line is reached.

Changing these files just requires you to stop the current TAS (with `Triangle`) and the starting another (with `L3`) to update. No rebuilds or any other changes are required - content is loading from `main.jaktas` at the start of each TAS run.

I normally work by adding `commands,frame-rate=999` at the top of my files, and then `commands,frame-rate=10` just before the sections that I'm working on so I can skip through the content and continue while analysing the frames.

I'm hoping to add a frame advance function in the future, but this will need to do until that's ready to go!

NOTE: If you add more than one of these in a line, only the last setting will be used.

### Inputs

The bread and butter of the TAS are the input lines, which are made up of a frame count and the buttons/controls to be used. These will make up 99% of every TAS file, but are pretty simple in their design. Some examples below:

```
13,leftx=140,lefty=0
10,L1
10,X
```

The first line will, for `13` total frames, hold the left analog stick (`leftx` and `lefty` for the `x` and `y` directions respectively) in a certain spot, followed by holding `L1` (which is crouch/roll) and then `X` (which is jump). That means that these commands will move in a certain direction, and then roll jump towards it.

Once the directions have been set those values will stay present, so you don't need to add `leftx=140` for every line as an example. If you want to reset it though, you'll need to add a line to do so (for example `leftx=128` to reset to neutral). This is similar behaviour to how the command lines work.

You can also chain multiple inputs in a single line (for example `10,Square,X,Circle` - Jak might not do much with them but they will go through all the same!

These inputs are made up of `leftx`/`lefty` (for the left analog stick), `rightx`/`righty` (for the right analog stick), and then buttons listed in `gamepad_map` in `game\kernel\common\kmachine.cpp` (which include controls like `Triangle`/`Circle`/`X`/`Square`). These currently must be capitalised correctly to work so please be careful!

NOTE: If you add more than one of these inputs in a line, only the last setting will be used.

# Workflow

For the moment we're a little lacking on the tooling side, with the above commands being all that I have to offer for functionality. I'm hoping to investigate options for displaying exact co-ordinates/direction/momentum as well as the ability to spawn in exact locations with these for faster TASing in lieu of save states, but I haven't investigated options for these yet. I have seen mods for OpenGOAL with similar functionality though, so hoping there are quick options!

In the meantime the way that I'm working is by importing all of the files I need in my `main.jaktas` file, with some speed up commands in place to skip through content I've already finished:

```
commands,frame-rate=999
import=util/quit-to-menu
import=util/create-new-game
commands,frame-rate=999
import=no-lts-geyser-rock
```

Then I just start editing the latest file I'm working on (in this case `no-lts-geyser-rock.jaktas` in order to progress. I use `commands,frame-rate=10` to slow things down while changing individual inputs for a second, and then move on. Occasionally you might hit issues running with higher frame-rates when they de-sync at the start, but I know roughly what's happening there and I don't think it'll be an issue in the future. It shouldn't happen at the target `60fps` either way.

Once the run has progressed significantly beyond Geyser this approach will need to change, but there are opportunities to just use file loads for most of the inputs and then join them up together once they're complete. For now I imagine this process and any bugs in this setup to be the biggest blockers to progress, but I'm actively looking to smooth them out.

# Closing notes

Thanks for reading this far - I hope you're now as keen as I am to see this TAS come to life! The Jak games have deserved these forever but historically the PS2 and emulators have been nightmares for TASing, where OpenGOAL has made the dream a reality. Looking forward to seeing what we can make of it!

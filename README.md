# doc2web
Create HTML help from a text file containing formatting commands

I use this app to generate the help for all of my projects that have help. The input is a plain text file containing formatting commands. The input file can also contain simple HTML, for example lists and tables. The best way to learn the syntax is to study one of my help files. I recommend the Polymeter project's [help file](https://github.com/victimofleisure/Polymeter/blob/main/docs/Help/help.txt) because it's the most recent and utilizes all the latest features. That single input file generates the compiled help file, the [online help tree](https://victimofleisure.github.io/Polymeter/helpframe.html), and the single-page [printable help](https://victimofleisure.github.io/Polymeter/Help/printable/PolymeterHelp.htm), thereby avoiding considerable parallel maintenance.

The project is a Microsoft Windows MFC command line app, compiled in Visual Studio 2012. The command line syntax is as follows. Note that the trailing optional arguments must be specified in the order shown. If you wish to skip an argument, specify it as nul.
```
doc2web [/numprefix] [/nospaces] [/logrefs] src_path dst_folder [contents_fname] [doc_path] [doc_title] [help_file_path]
```
|Argument|Description|Comment|
|--------|-----------|-------|
|numprefix|Prefixes topic and item names with sequential numbers.|Optional|
|nospaces|Replace spaces with underscores in anchor names.|Recommended|
|logrefs|Create a log file containing one line for each anchor along with the count of references to it.|Optional|
|src_path|The path of the input text file.|Required|
|dst_folder|The path of the output folder. To output to the current directory, use a period.|Required|
|contents_fname|The path of the table of contents file. If specified, a TOC linking to the topic pages is created.|Recommended|
|doc_path|The path of the printable help file. If specified, a single page containing all topics is created.|Optional|
|doc_title|The title of the help file.|Optional|
|help_file_path|The path of the HTML Help Project file in HHP format. This argument is only necessary when using HTML Help Workshop to create a compiled help file (CHM). If specified, the HHP file must already exist, and will be updated.|Optional|

Here are the actual command lines for the Polymeter project. The first command is for the CHM compiled help. The hhp file is both an input and an output; doc2web updates it, and the updated version is passed to HTML Help Workshop to build the CHM.
```
doc2web /nospaces /logrefs help.txt . nul printable.html "Polymeter Help" "Polymeter.hhp"
```
The second command is for the online HTML help. Note that this command creates two distinct versions: a help tree with each topic in its own page and each chapter in its own folder, and a single-file version suitable for printing.
```
doc2web /nospaces help.txt Help Contents.htm PolymeterHelp.htm "Polymeter Help"
```
# Commands

All commands start with an ampersand. **Unless otherwise specified, the ampersand must be the first character on the line**.

|Command Syntax|Description|
|-------|-----------|
|@chapter name|Starts a chapter. Each chapter must be terminated by a corresponding endchapter. Chapters can be nested.|
|@endchapter|Ends a chapter.|
|@topic name|Creates a topic within a chapter. Topics can NOT be nested.|
|@item name|Creates an item within a topic. Items can NOT be nested.|
|@[name]|Creates a link to a topic or item. Forward references are allowed. This command can appear anywhere in the line.|
|@{style}text@}|Formats *text* with *style*. The style must be defined in CSS. This command can appear anywhere in the line.|
|@{#name}|Creates an anchor for name. This command can appear anywhere in the line. It's useful for creating anchors in tables.|

Names are NOT case-sensitive and may contain spaces. For all commands that take a name parameter, the displayed text can optionally differ from the topic or item name. Specify the name first, then a backslash, then the text to display. For example:
```
@topic actual topic name\text to display
```

A very simple example:
```
@chapter Getting Started

@topic Introduction

This program does stuff. You'll want to @[installation\install] it first.

@topic Installation

@item Downloading

Go find the project somewhere and download it.

@item Running

Now run the thing you @[downloading\downloaded].  It does things as explained @[introduction\above].

@endchapter
```

To build the corresponding help, assuming the above is in help.txt:
```
doc2web help.txt . contents.html printable.html
```

Newlines in the input file are respected and indicate paragraphs. Put another way, each paragraph is a single line of text. Lines can be arbitarily long, so it's recommended to use a text editor that automatically wraps long lines. I use WordPad.


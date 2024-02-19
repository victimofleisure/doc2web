# doc2web
Create HTML help from a text file containing formatting commands

I use this app to generate the help for all of my projects that have help. The input is a plain text file containing formatting commands. The input file can also contain simple HTML, for example lists and tables. The best way to learn the syntax is to study one of my help files. I recommend the Polymeter project's [help file](https://github.com/victimofleisure/Polymeter/blob/main/docs/Help/help.txt) because it's the most recent and utilizes all the latest features.

The project is a Microsoft Windows MFC command line app, compiled in Visual Studio 2012. The command line syntax is:
```
doc2web [/numprefix] [/nospaces] [/logrefs] src_path dst_folder [contents_fname] [doc_path] [doc_title] [help_file_path]
```
Here are the actual command lines I use for the Polymeter project. The first command is for the CHM compiled help. The hhp file is both an input and an output; doc2web updates it, and the updated version is passed to HTML Help Workshop to build the CHM.
```
doc2web /nospaces /logrefs help.txt . nul printable.html "Polymeter Help" "Polymeter.hhp"
```
The second command is for the online HTML help. Note that this command creates two distinct versions: a help tree with each topic in its own page and each chapter in its own folder, and a single-file version.
```
doc2web /nospaces help.txt Help Contents.htm PolymeterHelp.htm "Polymeter Help"
```





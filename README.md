# doc2web
Create HTML help from a text file containing formatting commands

I use this code to generate the help for all of my projects that have it. The input is a plain text file containing formatting commands. The best way to learn the syntax is to study one of my help files. I recommend the Polymeter project's [help file](https://github.com/victimofleisure/Polymeter/blob/main/docs/Help/help.txt) because it's the most recent and utilizes all the latest features.

The project is Windows MFC command line app, compiled in Visual Studio 2012. The command line syntax is:
doc2web [/numprefix] [/nospaces] [/logrefs] src_path dst_folder [contents_fname] [doc_path] [doc_title] [help_file_path]

Here are the actual command lines I use for the Polymeter project.

For the CHM compiled help 
doc2web /nospaces /logrefs help.txt . nul printable.html "Polymeter Help" "Polymeter.hhp"

And for the online HTML help:
doc2web /nospaces help.txt Help Contents.htm PolymeterHelp.htm "Polymeter Help"

Note that the latter command creates two distinct versions: a help tree with each topic in its own page and each chapter in its own folder, and a single-file version.




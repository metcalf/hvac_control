To update font file:
fonttools varLib.mutator -o controller/squareline/assets/MaterialSymbolsOutlined.ttf controller/squareline/assets/MaterialSymbolsOutlined\[FILL\,GRAD\,opsz\,wght\].ttf wdth=400 GRAD=0 opsz=24

To add a symbol:

- Find the unicode codepoint for the symbol on Google Fonts
- Add it to the spreadsheet https://docs.google.com/spreadsheets/d/1Nw3WV2P3qzPx0jjchnZZa_tDFWB7HIrAM1buknjd-3g/edit?gid=373970532#gid=373970532
- Copy the top line of the sheet into the "range" for each version of the font in Squareline and click "modify"
- Copy to "Char" entry in the spreadsheet where you want in the UI

https://espressif-docs.readthedocs-hosted.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html

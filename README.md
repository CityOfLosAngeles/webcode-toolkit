# WebCode Toolkit

### Contents of this file
* Overview
* Features
* Installation
* Configuration options
* Demo
* Authors
* License

## Overview
The Los Angeles Department of City Planning is embarking on one of the City’s largest planning initiatives to date: re:code LA, a comprehensive revision of LA’s outdated zoning code. First adopted in 1946, the current Code has grown from a simple, 84-page pamphlet to an unwieldy, 600+ page book that inadequately realizes a 21st Century vision of a better Los Angeles for all residents. 

Learn more about the project at http://recode.la/. 

One deliverable of this project is a Web-Based Zoning Code, which provides an online and interactive experience to browse, search, and download the new zoning code.

In an effort to contribute key components of this Web-Based Zoning Code project to the open source community, the Los Angeles Department of City Planning has authorized the WebCode Toolkit to be made available to the public.

## Features

### Epub Export Plugin
This script automates the process to export the content in the InDesign file as an Epub format.

### Epub Import Module
This module allows an Epub file to be uploaded to a Drupal site and have content nodes generated with the text and images content contained within.  

### InDesign Tagging
InDesign allows an editor to include a class to specific tags in the document styles. This is done from the Export Tagging screen on the Character Style and Paragraph Style options menu. Using this, it is possible to send custom metadata about the InDesign document to Drupal. The following classes are currently supported:

* ol for ordered lists
* ul for unordered lists

## Installation
The use of the WebCode Toolkit requires both a copy of InDesign and a Drupal website installed. To learn more about these applications please visit the following sites:

* http://www.adobe.com/indesign/
* http://www.drupal.org/

### Directory structure
```
epub_export/    = Location of the InDesign script
epub_import/    = Drupal module
```

### InDesign
To install the InDesign script, place a copy of the EpubExport.jsx file into the Scripts Panel folder inside the InDesign application folder. 

Additional information can be found here:
* https://helpx.adobe.com/indesign/using/scripting.html

### Drupal
To install the Drupal module, place the epub_import folder and all of it's contents into the sites/all/modules/ directory of your Drupal installation. Then you need to enable the module on the site's Module Administration page (/admin/modules).

Additional information can be found here:
* https://www.drupal.org/docs/7/extending-drupal-7/installing-drupal-7-contributed-modules

Once installed, Epub files can be imported into Drupal from here:
* /admin/epub-import

## Configuration options

### InDesign HTML Tags
Using InDesign’s own features you can define HTML elements that get included in the Epub file from the character styles. While in the Character Styles window click the Export Tagging menu link to customize the options for "Epub and HTML”. 

* Tag: Select “span” to properly export the tags to the Epub file.
* Class: Insert a class name you want included with this particular character style. 

### Drupal Module
Once enabled, the Drupal module offers the following ways to further customize the feature:

* Import Epub as: Select the content type that should be used when an Epub file is imported.
* Text format: Select the default Drupal text format for imported text content.
* Image style: Select the Drupal image style that will be used for imported image content.
* Images folder: Select the file directory that will be used for imported image content.
* Header level for each content: Select the InDesign header style that will be used as the break point for each node of content when the Epub file is imported.
* Taxonomy vocabulary: Select the Drupal taxonomy vocabulary that will be used for imported nodes.
* Class name prefix: Customize the prefix used for taxonomy terms in the Epub file.

## Demo
Coming soon!

## Authors
* Ki Kim (kkim@urbaninsight.com)
* Mark Etter (etter@urbaninsight.com)

## License
Released under the GNU General Public License, version 2 (or higher).

Copyright (c) 2017 City of Los Angeles.
/**
 * JSX to export Indesign to Epub
 *
 * Author: Ki Kim, Urban Insight
 * Date: 09/27/2017
 * Version: 1.0
 *
 * Ref: http://wwwimages.adobe.com/content/dam/Adobe/en/devnet/indesign/sdk/cs6/scripting/InDesign_ScriptingGuide_JS.pdf
 */

Date.prototype.yyyymmdd = function() {
  var yyyy = this.getFullYear().toString();
  var mm = (this.getMonth()+1).toString(); // getMonth() is zero-based
  var dd  = this.getDate().toString();
  return yyyy + (mm[1]?mm:"0"+mm[0]) + (dd[1]?dd:"0"+dd[0]); // padding
};

var myDocument = app.documents.item(0);
var today = (new Date()).yyyymmdd();

var desktop = Folder.desktop.fullName;
var fileName = today + '.epub';

with (myDocument.epubExportPreferences) {
  bulletExportOption = BulletListExportOption.AS_TEXT;
  numberedListExportOption = NumberedListExportOption.AS_TEXT;
}

myDocument.exportFile(ExportFormat.EPUB, File(desktop + '/' + fileName), false);

//Sets EPub export options, then exports the active document as EPub.
/*
with (myDocument.epubExportPreferences) {
  //Apply image alignment to anchored object settings.
  applyImageAlignmentToAnchoredObjectSettings = false;

  //The unit of space.
  spaceUnit = SpaceUnitType.CSS_EM;

  //The unit of margin.
  marginUnit = SpaceUnitType.CSS_EM;

  //Bottom margin of the epub.
  bottomMargin = 5;

  //Left margin of the epub.
  leftMargin = 5;

  //Right margin of the epub.
  rightMargin = 5;

  //Top margin of the epub.
  topMargin = 5;

  //If true, break InDesign document into smaller pieces when generating epub.
  breakDocument = false;
  //The name of paragraph style to break InDesign document.
  //paragraphStyleName = "";

  //The bullet export option.
  bulletExportOption = BulletListExportOption.AS_TEXT;

  cssExportOption = StyleSheetExportOption.EMBEDDED_CSS;
  customImageSizeOption = ImageSizeOption.SIZE_RELATIVE_TO_PAGE_WIDTH;

  embedFont = true;
  epubCover = EpubCover.FIRST_PAGE;
  //This will take effect only when epubCover is set to EXTERNAL_IMAGE.
  //coverImageFile = "/c/cover.jpg";

  epubPublisher = "Adobe Devtech";

  //The export order.
  exportorder = ExportOrder.LAYOUT_ORDER;

  //If true, output footnote immediately after its paragraph.
  footnoteFollowParagraph = false;

  //If true, export epub in XHTML format. Otherwise, in DTBook format.
  format = true;

  gifOptionsInterlaced = true;
  gifOptionsPalette = GIFOptionsPalette.WINDOWS_PALETTE;

  //The epub unique identifier, like ISBN.
  id = "123";

  //Ignore object level image conversion settings.
  ignoreObjectConversionSettings = true;

  //Alignment applied to images.
  imageAlignment = ImageAlignmentType.ALIGN_CENTER;

  //The file format to use for converted images.
  //Valid only when copy optimized images and/or copy formatted images is true.
  imageConversion = ImageConversion.AUTOMATIC;
  imageExportResolution = ImageResolution.PPI_150;
  //Image page break settings to be used with objects.
  imagePageBreak = ImagePageBreakType.PAGE_BREAK_AFTER;
  //Space After applied to images.
  imageSpaceAfter = 2;
  //Space Before applied to images.
  imageSpaceBefore = 2;

  //If true, include CSS definition.
  includeCSSDefinition = true;
  //If true, output document metadata into epub.
  includeDocumentMetadata = true;

  jpegOptionsFormat = JPEGOptionsFormat.BASELINE_ENCODING;
  jpegOptionsQuality = JPEGOptionsQuality.HIGH;

  //The PNG compression level.
  level = 5;

  numberedListExportOption = NumberedListExportOption.AS_TEXT;

  //If true, format image based on layout appearence.
  preserveLayoutAppearence = true;
  //If true, output local style override.
  preserveLocalOverride = true;

  stripSoftReturn = true;
  //If true, image page break settings will be used in objects.
  useImagePageBreak = true;
  //Use InDesign TOC style to generate epub TOC.
  useTocStyle = true;

  viewDocumentAfterExport = false;
}
*/

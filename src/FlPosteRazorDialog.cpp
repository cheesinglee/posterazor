/*
	PosteRazor - Make your own poster!
	Copyright (C) 2005-2011 by Alessandro Portale
	http://posterazor.sourceforge.net/

	This file is part of PosteRazor

	PosteRazor is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	
	PosteRazor is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with PosteRazor; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "FlPosteRazorDialog.h"
#include "FlPosteRazorHelpDialog.h"
#include "PosteRazorWizardDialogController.h"
#include "FlPersistentPreferences.h"
#include <FL/filename.H>
#include <FL/fl_ask.H>
#include <FL/x.H>
#include <FL/Fl_Native_File_Chooser.H>
#include "TranslationConstants.h"


#if defined (WIN32)
  #include <math.h>
  #include <io.h>
  #include "PosteRazorResource.h"
  #define CASESENSITIVESTRCMP stricmp
  #define CASESENSITIVESTRNCMP strnicmp
#else
  #include <iostream>
  #include <unistd.h>
  #define CASESENSITIVESTRCMP strcasecmp
  #define CASESENSITIVESTRNCMP strncasecmp
#endif

#if !defined (WIN32) && !defined(__APPLE__)
  #include <X11/xpm.h>
  #include "PosteRazor.xpm"
#endif

const char PreferencesVendor[] = "CasaPortale.de";
const char PreferencesProduct[] = "PosteRazor";
const char preferencesKey_UseOpenGLForPreview[] = "UseOpenGLForPreview";
const char preferencesKey_LoadImageChooserLastPath[] = "LoadImageChooserLastPath";
const char preferencesKey_SavePosterChooserLastPath[] = "SavePosterChooserLastPath";
const char preferencesKey_UILanguage[] = "UILanguage";

FlPosteRazorDragDropWidget::FlPosteRazorDragDropWidget(int x, int y, int w, int h, const char *label)
	:Fl_Box(FL_NO_BOX, x, y, w, h, label)
{
}

int FlPosteRazorDragDropWidget::handle(int event)
{
	switch (event)
	{
	case FL_DND_ENTER:
	case FL_DND_DRAG:
	case FL_DND_LEAVE:
	case FL_DND_RELEASE:
		return 1;
	case FL_PASTE:
		return parent()->handle(event);
	default:
		return 0;
	};
}

FlPosteRazorDialog::FlPosteRazorDialog(void)
	: PosteRazorDialogUI(620, 455, "PosteRazor "POSTERAZORVERSION)
	, m_settingsDialog(NULL)
	, m_helpDialog(NULL)
{
	int i;
#if defined(WIN32) || defined(__APPLE__)
// Drag & Drop doesn't work on Linux. That is dragging a file from Konqueror just gives
// us the filename without path but with an appended character with ascii value 10.
// Also, we sometimes get a "X_ConvertSelection: BadAtom (invalid Atom parameter) 0x2" message on stderr
// May work with other file managers
	begin();
	m_dragDropWidget = new FlPosteRazorDragDropWidget(0, 0, w(), h());
	end();
#endif
	m_wizard->value(m_loadInputImageStep);

	m_posteRazor = PosteRazor::createPosteRazor();
	m_posteRazorController = new PosteRazorWizardDialogController();
	m_posteRazorController->setPosteRazorWizardDialog(this);
	m_posteRazorController->setPosteRazorModel(m_posteRazor);

	FlPersistentPreferences preferences(PreferencesVendor, PreferencesProduct);
	m_posteRazorController->readPersistentPreferences(&preferences);
	const FlPaintCanvasGroup::ePaintCanvasTypes paintCanvasType =
		preferences.getBoolean(preferencesKey_UseOpenGLForPreview, getUseOpenGLForPreviewByDefault())?FlPaintCanvasGroup::PaintCanvasTypeGL:FlPaintCanvasGroup::PaintCanvasTypeDraw;
	Translations::eLanguages language = (Translations::eLanguages)preferences.getInteger(preferencesKey_UILanguage, Translations::eLanguageUndefined);
	m_UILanguageIsUndefined = language == Translations::eLanguageUndefined;
	if (m_UILanguageIsUndefined)
		language = Translations::getSystemLanguage();
	Translations::instance().selectLangue(language);

	strncpy(m_loadImageChooserLastPath, preferences.getString(preferencesKey_LoadImageChooserLastPath, ""), sizeof(m_loadImageChooserLastPath));
	m_loadImageChooserLastPath[sizeof(m_loadImageChooserLastPath) - 1] = '\0';
	strncpy(m_savePosterChooserLastPath, preferences.getString(preferencesKey_SavePosterChooserLastPath, ""), sizeof(m_savePosterChooserLastPath));
	m_savePosterChooserLastPath[sizeof(m_savePosterChooserLastPath) - 1] = '\0';

	const int paperFormatMenuItemsCount = PaperFormats::getPaperFormatsCount()+1;
	m_paperFormatMenuItems = new Fl_Menu_Item[paperFormatMenuItemsCount];
	memset(m_paperFormatMenuItems, 0, sizeof(Fl_Menu_Item)*paperFormatMenuItemsCount);
	for (i = 0; i < PaperFormats::getPaperFormatsCount(); i++) {
		const char* const paperFormatName = PaperFormats::getPaperFormatName(PaperFormats::getPaperFormatForIndex(i));
		m_paperFormatMenuItems[i].label(paperFormatName);
		m_paperFormatMenuItems[i].callback(handlePaperFormatChoice_cb);
		m_paperFormatMenuItems[i].user_data((void*)this);
	}
	m_paperFormatChoice->menu(m_paperFormatMenuItems);

	m_imageInfoGroup->deactivate();

	m_paintCanvasGroup->setPaintCanvasType(paintCanvasType);
	m_paintCanvasGroup->setPainterInterface(m_posteRazor);

	m_settingsButton->labelImage(m_settingsButtonLabel->image());

#if defined(NO_LAUNCH_URLS_OR_FILES)
	m_setLaunchPDFApplicationCheckButton->hide();
	m_savePosterGroup->resizable(m_setLaunchPDFApplicationCheckButton);
	m_savePosterGroup->size(m_savePosterGroup->w(),  m_savePosterGroup->h()-m_setLaunchPDFApplicationCheckButton->h());
#endif

	updateLanguage();
}

static const char* GetPathFromFileName(const char* fileName)
{
	static char pathName[1024] = "";
	
	strncpy(pathName, fileName, sizeof(pathName));
	pathName[sizeof(pathName) - 1] = '\0';
	
	pathName[strlen(pathName) - strlen(fl_filename_name(pathName))] = '\0';
	
	return pathName;
}

FlPosteRazorDialog::~FlPosteRazorDialog()
{
	FlPersistentPreferences preferences(PreferencesVendor, PreferencesProduct);
	m_posteRazorController->writePersistentPreferences(&preferences);
	preferences.setBoolean(m_paintCanvasGroup->getPaintCanvasType() == FlPaintCanvasGroup::PaintCanvasTypeGL, preferencesKey_UseOpenGLForPreview);
	preferences.setString(m_loadImageChooserLastPath, preferencesKey_LoadImageChooserLastPath);
	preferences.setString(m_savePosterChooserLastPath, preferencesKey_SavePosterChooserLastPath);
	if (m_UILanguageIsUndefined)
		Translations::instance().selectLangue(Translations::eLanguageUndefined);
	preferences.setInteger(Translations::instance().getSelectedLanguage(), preferencesKey_UILanguage);

	if (m_paperFormatMenuItems)
		delete[] m_paperFormatMenuItems;

	if (m_settingsDialog)
		delete m_settingsDialog;

	if (m_helpDialog)
		delete m_helpDialog;
}

int FlPosteRazorDialog::handle(int event)
{
	switch (event)
	{
	case FL_PASTE:
		loadInputImage(Fl::event_text());
		return 1;
	default:
		return PosteRazorDialogUI::handle(event);
	};
}

bool FlPosteRazorDialog::getUseOpenGLForPreviewByDefault(void)
{
#if defined(WIN32)
	// For some reasons, some Windows 98 systems had problems with OpenGL. Maybe all non-NT versions?
	OSVERSIONINFO osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
	return (osvi.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS);
#else
	return true;
#endif
}

void FlPosteRazorDialog::openSettingsDialog(void)
{
	if (!m_settingsDialog) {
		m_settings.unitOfLength = m_posteRazor->getUnitOfLength();
		m_settings.previewType = m_paintCanvasGroup->getPaintCanvasType();
		m_settings.language = Translations::instance().getSelectedLanguage()!=Translations::eLanguageUndefined?Translations::instance().getSelectedLanguage():Translations::eLanguageEnglish;
		m_settingsDialog = new FlPosteRazorSettingsDialog();
		m_settingsDialog->set_modal();
	}
	
	m_settingsDialog->setOptionsAndHandler(&m_settings, this);
	
	posteRazorSettings settingsbackup = m_settings;
	const bool languageIsUndefinedBackup = m_UILanguageIsUndefined;
	if (!m_settingsDialog->show_modal()) {
		handleOptionsChangement(&settingsbackup);
		m_UILanguageIsUndefined = languageIsUndefinedBackup;
	}
}

void FlPosteRazorDialog::openHelpDialog(void)
{
	if (!m_helpDialog) {
		m_helpDialog = new FlPosteRazorHelpDialog();
		m_helpDialog->updateLanguage();
		m_helpDialog->set_modal();
	}

	char stepTopic[1024];
	sprintf(stepTopic, POSTERAZORHELPANCHORMANUALSTEP "%.2d", getCurrentWizardStepNumber() + 1);

	m_helpDialog->setHtmlContent(Translations::instance().helpHtml());
	m_helpDialog->jumpToAnchor(stepTopic);
	m_helpDialog->show();
}

void FlPosteRazorDialog::handleOptionsChangement(posteRazorSettings *settings)
{
	m_posteRazorController->setUnitOfLength(settings->unitOfLength);
	m_paintCanvasGroup->setPaintCanvasType(settings->previewType);

	if (Translations::instance().getSelectedLanguage() != settings->language) {
		Translations::instance().selectLangue(settings->language);
		m_UILanguageIsUndefined = false;
		updateLanguage();
	}
	m_settings = *settings;
}

void FlPosteRazorDialog::next(void)
{
	m_posteRazorController->handleNextButtonPressed();
}

void FlPosteRazorDialog::prev(void)
{
	m_posteRazorController->handlePrevButtonPressed();
}

void FlPosteRazorDialog::updateNavigationButtons(void)
{
}

const char* FlPosteRazorDialog::getWizardStepInfoString(PosteRazorWizardDialogEnums::ePosteRazorWizardSteps step)
{
	return (
		step == PosteRazorWizardDialogEnums::ePosteRazorWizardStepInputImage?Translations::instance().stepTitle01()
		:step == PosteRazorWizardDialogEnums::ePosteRazorWizardStepPaperSize?Translations::instance().stepTitle02()
		:step == PosteRazorWizardDialogEnums::ePosteRazorWizardStepOverlapping?Translations::instance().stepTitle03()
		:step == PosteRazorWizardDialogEnums::ePosteRazorWizardStepPosterSize?Translations::instance().stepTitle04()
		:Translations::instance().stepTitle05()
	);
}

int FlPosteRazorDialog::getCurrentWizardStepNumber(void)
{
	return (
		m_wizard->value() == m_loadInputImageStep?0
		:m_wizard->value() == m_paperSizeStep?1
		:m_wizard->value() == m_overlappingStep?2
		:m_wizard->value() == m_posterSizeStep?3
		:4
	);
}

void FlPosteRazorDialog::updateStepInfoBar(PosteRazorWizardDialogEnums::ePosteRazorWizardSteps step)
{
	char stepStr[1024];
	sprintf(stepStr, Translations::instance().stepXOfY(), getCurrentWizardStepNumber() + 1, 5); 
	char helpTitleStr[1024];
	sprintf(helpTitleStr, "  %s: %s", stepStr, getWizardStepInfoString(step));
	m_stepInfoBox->copy_label(helpTitleStr);
}

void FlPosteRazorDialog::updatePreview(void)
{
	m_paintCanvasGroup->redraw();
}

void FlPosteRazorDialog::updateLanguage(void)
{
	char tempStr[1024];

	sprintf(tempStr, "%s @-2->", Translations::instance().next());
	m_nextButton->copy_label(tempStr);
	sprintf(tempStr, "@-2<- %s", Translations::instance().back());
	m_prevButton->copy_label(tempStr);

	m_paperFormatTypeTabs->label(Translations::instance().paperFormat());
	m_paperFormatCustomGroup->label(Translations::instance().custom());
	m_paperFormatStandardGroup->label(Translations::instance().standard());
	m_paperFormatChoice->label(Translations::instance().format());
	m_paperOrientationGroup->label(Translations::instance().orientation());
	m_paperOrientationLandscapeRadioButton->label(Translations::instance().landscape());
	m_paperOrientationPortraitRadioButton->label(Translations::instance().portrait());
	m_paperCustomHeightInput->label(Translations::instance().height());
	m_paperCustomWidthInput->label(Translations::instance().width());
	m_paperBorderTopInput->label(Translations::instance().top());
	m_paperBorderRightInput->label(Translations::instance().right());
	m_paperBorderBottomInput->label(Translations::instance().bottom());
	m_paperBorderLeftInput->label(Translations::instance().left());

	m_overlappingHeightInput->label(Translations::instance().height());
	m_overlappingWidthInput->label(Translations::instance().width());
	m_overlappingPositionGroup->label(Translations::instance().overlappingPosition());
	m_overlappingSizeGroup->label(Translations::instance().overlappingSize());
	m_overlappingPositionBottomLeftButton->label(Translations::instance().bottomLeft());
	m_overlappingPositionBottomRightButton->label(Translations::instance().bottomRight());
	m_overlappingPositionTopLeftButton->label(Translations::instance().topLeft());
	m_overlappingPositionTopRightButton->label(Translations::instance().topRight());

	m_posterSizeGroup->label(Translations::instance().imageSize());
	m_posterSizeAbsoluteRadioButton->label(Translations::instance().absoluteSize());
	m_posterAbsoluteHeightInput->label(Translations::instance().height());
	m_posterAbsoluteWidthInput->label(Translations::instance().width());
	m_posterSizeInPagesRadioButton->label(Translations::instance().sizeInPages());
	m_posterPagesHeightInput->label(Translations::instance().height());
	m_posterPagesHeightLabel->label(Translations::instance().pages());
	m_posterPagesWidthInput->label(Translations::instance().width());
	m_posterPagesWidthLabel->label(Translations::instance().pages());
	m_posterSizePercentualRadioButton->label(Translations::instance().sizeInPercent());
	m_posterPercentualSizeInput->label(Translations::instance().Size());
	m_imageAlignmentGroup->label(Translations::instance().imageAlignment());
	m_imageInfoGroup->label(Translations::instance().imageInformations());
	m_imageLoadGroup->label(Translations::instance().inputImage());
	m_settingsButton->label(Translations::instance().settings());

	m_savePosterGroup->label(Translations::instance().saveThePoster());
	m_setLaunchPDFApplicationCheckButton->label(Translations::instance().launchPDFApplication());

	m_posteRazorController->updateDialog();

	if (m_settingsDialog)
		m_settingsDialog->updateLanguage();

	if (m_helpDialog)
		m_helpDialog->updateLanguage();

	redraw();
}

void FlPosteRazorDialog::getFileOpenDialogFilter(char *filter, int bufferLength)
{
	char filterString[1024] = "";
	char allExtensions[1024] = "";
	char formats[1024] = "%s\t*.{%s}";

	for (int formatIndex = 0; formatIndex < ImageIOTypes::getInputImageFormatsCount(); formatIndex++) {
		char extensionsOfFormat[1024] = "";
		char format[1024] = "";

		const int extensionsCount = ImageIOTypes::getFileExtensionsCount(formatIndex);

		for (int extensionIndex = 0; extensionIndex < extensionsCount; extensionIndex++) {
			if (extensionIndex != 0)
				strcat(extensionsOfFormat, ", ");
			strcat(extensionsOfFormat, ImageIOTypes::getFileExtensionForFormat(extensionIndex, formatIndex));
		}
		if (formatIndex != 0)
			strcat(allExtensions, ", ");
		strcat(allExtensions, extensionsOfFormat);
		sprintf(format, "\n%s (%s)\t*.{%s}", ImageIOTypes::getInputImageFormat(formatIndex), extensionsOfFormat, extensionsOfFormat);
		strcat(formats, format);
	}

	sprintf	(
		filterString,
		formats,
		Translations::instance().allImageFormats(),
		allExtensions
	);

	strncpy(filter, filterString, bufferLength);
	filter[bufferLength-1] = '\0';
}

void FlPosteRazorDialog::loadInputImage(const char *fileName)
{
	char errorMessage[1024] = "";
	const char *loadFileName = fileName;
	bool loaded = false;

	Fl_Native_File_Chooser loadImageChooser(Fl_Native_File_Chooser::BROWSE_FILE);
#if defined (WIN32)
// filter stuff is still crashy os OSX and ugly on Linux
	OSVERSIONINFO osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);

// It apparently does also not work in Windows Vista
	if (osvi.dwMajorVersion < 6) {
		char filterString[1024];
		getFileOpenDialogFilter(filterString, sizeof(filterString));
		loadImageChooser.filter(filterString);
	}
#endif
	loadImageChooser.title(Translations::instance().loadAnInputImage());

	if (!loadFileName) {
		loadImageChooser.directory(m_loadImageChooserLastPath);
		if (loadImageChooser.show() == 0) {
			loadFileName = loadImageChooser.filename();

			strncpy(m_loadImageChooserLastPath, GetPathFromFileName(loadFileName), sizeof(m_loadImageChooserLastPath));
			m_loadImageChooserLastPath[sizeof(m_loadImageChooserLastPath)-1] = '\0';
		}
	}

	if (loadFileName) {
		loaded = m_posteRazorController->loadInputImage(loadFileName, errorMessage, sizeof(errorMessage));
		if (!loaded) {
			if (strlen(errorMessage) > 0)
				fl_message("%s", errorMessage);
			else
				fl_message(Translations::instance().fileCouldNotBeLoaded(), fl_filename_name(loadFileName));
		}
	}

	if (loaded) {
		m_imageInfoGroup->activate();
		m_paintCanvasGroup->requestImage();
	}

	updateNavigationButtons();
	Fl::flush(); // Needed for windows if image is loaded at startup
}

void FlPosteRazorDialog::showImageFileName(const char *fileName)
{
	m_inputFileNameLabel->copy_label(fl_filename_name(fileName));
}

void FlPosteRazorDialog::updateImageInfoFields(int imageWidthInPixels, int imageHeightInPixels, double imageWidth, double imageHeight, UnitsOfLength::eUnitsOfLength unitOfLength, double verticalDpi, double horizontalDpi, ColorTypes::eColorTypes colorType, int bitsPerPixel)
{
	char sizeInDimensionUnitString[100];
	sprintf(sizeInDimensionUnitString, Translations::instance().sizeInUnitOfLength(), UnitsOfLength::getUnitOfLengthName(unitOfLength));

	char string[1024];

	sprintf (
		string,
		"%s\n%s\n%s\n%s",
		Translations::instance().sizeInPixels(),
		sizeInDimensionUnitString,
		Translations::instance().resolution(),
		Translations::instance().colorType()
	);
	m_imageInfoKeysLabel->copy_label(string);

	sprintf (
		string,
		"%d x %d\n%.2f x %.2f\n%.1f dpi\n%s %dbpp",
		imageWidthInPixels, imageHeightInPixels,
		imageWidth, imageHeight,
		verticalDpi,
			colorType==ColorTypes::eColorTypeMonochrome?Translations::instance().monochrome():
			colorType==ColorTypes::eColorTypeGreyscale?Translations::instance().grayscale():
			colorType==ColorTypes::eColorTypePalette?Translations::instance().palette():
			colorType==ColorTypes::eColorTypeRGB?"RGB":
			colorType==ColorTypes::eColorTypeRGBA?"RGBA":
			/*colorType==eColorTypeCMYK?*/ "CMYK",
		bitsPerPixel
	);
	m_imageInfoValuesLabel->copy_label(string);
}

void FlPosteRazorDialog::updatePosterSizeFields(Fl_Widget *sourceWidget)
{
	if (sourceWidget == m_posterAbsoluteWidthInput)
		m_posteRazorController->setPosterWidth(PosteRazorEnums::ePosterSizeModeAbsolute, m_posterAbsoluteWidthInput->value());
	else if (sourceWidget == m_posterAbsoluteHeightInput)
		m_posteRazorController->setPosterHeight(PosteRazorEnums::ePosterSizeModeAbsolute, m_posterAbsoluteHeightInput->value());
	else if (sourceWidget == m_posterPagesWidthInput)
		m_posteRazorController->setPosterWidth(PosteRazorEnums::ePosterSizeModePages, m_posterPagesWidthInput->value());
	else if (sourceWidget == m_posterPagesHeightInput)
		m_posteRazorController->setPosterHeight(PosteRazorEnums::ePosterSizeModePages, m_posterPagesHeightInput->value());
	else // if (sourceWidget == m_posterPercentualSizeInput)
		m_posteRazorController->setPosterWidth(PosteRazorEnums::ePosterSizeModePercentual, m_posterPercentualSizeInput->value());
}

void FlPosteRazorDialog::handlePaperFormatChoice_cb(Fl_Widget *widget, void *userData)
{
	((FlPosteRazorDialog*)userData)->handlePaperSizeChangement(((FlPosteRazorDialog*)userData)->m_paperFormatChoice);
}

void FlPosteRazorDialog::handlePaperSizeChangement(Fl_Widget *sourceWidget)
{
	// TODO: Convert to switch
	if (sourceWidget == m_paperBorderTopInput)
		m_posteRazorController->setPaperBorderTop(m_paperBorderTopInput->value());
	else if (sourceWidget == m_paperBorderRightInput)
		m_posteRazorController->setPaperBorderRight(m_paperBorderRightInput->value());
	else if (sourceWidget == m_paperBorderBottomInput)
		m_posteRazorController->setPaperBorderBottom(m_paperBorderBottomInput->value());
	else if (sourceWidget == m_paperBorderLeftInput)
		m_posteRazorController->setPaperBorderLeft(m_paperBorderLeftInput->value());
	else if (sourceWidget == m_paperFormatTypeTabs)
		m_posteRazorController->setUseCustomPaperSize(m_paperFormatTypeTabs->value() == m_paperFormatCustomGroup);
	else if (sourceWidget == m_paperFormatChoice) {
		const char* const paperFormatName = m_paperFormatMenuItems[m_paperFormatChoice->value()].label();
		m_posteRazorController->setPaperFormatByName(paperFormatName);
	}
	else if (sourceWidget == m_paperOrientationPortraitRadioButton || sourceWidget == m_paperOrientationLandscapeRadioButton)
		m_posteRazorController->setPaperOrientation(m_paperOrientationLandscapeRadioButton->value() != 0?PaperFormats::ePaperOrientationLandscape:PaperFormats::ePaperOrientationPortrait);
	else if (sourceWidget == m_paperCustomWidthInput)
		m_posteRazorController->setCustomPaperWidth(m_paperCustomWidthInput->value());
	else if (sourceWidget == m_paperCustomHeightInput)
		m_posteRazorController->setCustomPaperHeight(m_paperCustomHeightInput->value());
}

void FlPosteRazorDialog::handleOverlappingChangement(Fl_Widget *sourceWidget)
{
	// TODO: Convert to switch
	if (sourceWidget == m_overlappingWidthInput)
		m_posteRazorController->setOverlappingWidth(m_overlappingWidthInput->value());
	else if (sourceWidget == m_overlappingHeightInput)
		m_posteRazorController->setOverlappingHeight(m_overlappingHeightInput->value());
	else
		m_posteRazorController->setOverlappingPosition (
			m_overlappingPositionBottomRightButton->value()?PosteRazorEnums::eOverlappingPositionBottomRight
			:m_overlappingPositionBottomLeftButton->value()?PosteRazorEnums::eOverlappingPositionBottomLeft
			:m_overlappingPositionTopLeftButton->value()?PosteRazorEnums::eOverlappingPositionTopLeft
			:PosteRazorEnums::eOverlappingPositionTopRight
		);
}

void FlPosteRazorDialog::updatePosterSizeGroupsState(void)
{
	m_posterSizeAbsoluteRadioButton->value() == 0?m_posterSizeAbsoluteGroup->deactivate():m_posterSizeAbsoluteGroup->activate();
	m_posterSizeInPagesRadioButton->value() == 0?m_posterSizeInPagesGroup->deactivate():m_posterSizeInPagesGroup->activate();
	m_posterSizePercentualRadioButton->value() == 0?m_posterSizePercentualGroup->deactivate():m_posterSizePercentualGroup->activate();
}

void FlPosteRazorDialog::handlePosterImageAlignment(void)
{
	m_posteRazorController->setPosterVerticalAlignment (
		m_posterAlignmentTopButton->value() == 1?PosteRazorEnums::eVerticalAlignmentTop
		:m_posterAlignmentMiddleButton->value() == 1?PosteRazorEnums::eVerticalAlignmentMiddle
		:PosteRazorEnums::eVerticalAlignmentBottom
	);
	
	m_posteRazorController->setPosterHorizontalAlignment (
		m_posterAlignmentLeftButton->value() == 1?PosteRazorEnums::eHorizontalAlignmentLeft
		:m_posterAlignmentCenterButton->value() == 1?PosteRazorEnums::eHorizontalAlignmentCenter
		:PosteRazorEnums::eHorizontalAlignmentRight
	);
}

// dirty 
static bool my_file_exists(const char* fileName)
{
	return (access(fileName, 0) == 0);
}

void FlPosteRazorDialog::savePoster(void)
{
	char saveFileName[1024] = "";
	bool fileExistsAskUserForOverwrite = false;
	
	Fl_Native_File_Chooser savePosterChooser(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
#ifndef __APPLE__
// filter stuff is still crashy os OSX
	savePosterChooser.filter("Portable Document format (*.PDF)\t*.pdf");
#endif
	savePosterChooser.title(Translations::instance().saveThePosterAs());

	do {
		if (fileExistsAskUserForOverwrite)
			savePosterChooser.preset_file(fl_filename_name(saveFileName));

		savePosterChooser.directory(m_savePosterChooserLastPath);
		if (savePosterChooser.show() == 0) {
			strcpy(saveFileName, savePosterChooser.filename());

			strncpy(m_savePosterChooserLastPath, GetPathFromFileName(saveFileName), sizeof(m_savePosterChooserLastPath));
			m_savePosterChooserLastPath[sizeof(m_savePosterChooserLastPath) - 1] = '\0';

			if (0 != CASESENSITIVESTRCMP(fl_filename_ext(savePosterChooser.filename()), ".pdf"))
				strcat(saveFileName, ".pdf");

			fileExistsAskUserForOverwrite = my_file_exists(saveFileName);
			char overwriteQuestion[1024] = "";
			sprintf(overwriteQuestion, Translations::instance().overwriteFile(), fl_filename_name(saveFileName));
			if (!fileExistsAskUserForOverwrite || fl_choice("%s", fl_no, fl_yes, 0, overwriteQuestion) == 1) {
				int err = m_posteRazorController->savePoster(saveFileName);
				if (err)
					fl_message(Translations::instance().fileCouldNotBeSaved(), fl_filename_name(saveFileName));
				fileExistsAskUserForOverwrite = false;
			}
		}
		else
			break;
	} while (fileExistsAskUserForOverwrite);
}

void FlPosteRazorDialog::setLaunchPDFApplication(void)
{
	m_posteRazorController->setLaunchPDFApplication(m_setLaunchPDFApplicationCheckButton->value()==0?false:true);
}

void FlPosteRazorDialog::setUnitOfLength(UnitsOfLength::eUnitsOfLength unit)
{
	const char* const unitName = UnitsOfLength::getUnitOfLengthName(unit);
	m_paperCustomWidthDimensionUnitLabel->label(unitName);
	m_paperCustomHeightDimensionUnitLabel->label(unitName);
	m_overlappingWidthDimensionUnitLabel->label(unitName);
	m_overlappingHeightDimensionUnitLabel->label(unitName);
	m_posterAbsoluteWidthDimensionUnitLabel->label(unitName);
	m_posterAbsoluteHeightDimensionUnitLabel->label(unitName);
	char paperBordersGroupLabel[100];
	sprintf(paperBordersGroupLabel, "%s (%s)", Translations::instance().borders(), unitName);
	m_paperBordersGroup->copy_label(paperBordersGroupLabel);
}

void FlPosteRazorDialog::setPaperFormat(PaperFormats::ePaperFormats format)
{
	m_paperFormatChoice->value((int)format);
}

void FlPosteRazorDialog::setPaperOrientation(PaperFormats::ePaperOrientations orientation)
{
	// standard paper format
	m_paperOrientationPortraitRadioButton->value(orientation == PaperFormats::ePaperOrientationPortrait);
	m_paperOrientationLandscapeRadioButton->value(orientation == PaperFormats::ePaperOrientationLandscape);
}

void FlPosteRazorDialog::setPaperBorderTop(double border)
{
	m_paperBorderTopInput->value(border);
}

void FlPosteRazorDialog::setPaperBorderRight(double border)
{
	m_paperBorderRightInput->value(border);
}

void FlPosteRazorDialog::setPaperBorderBottom(double border)
{
	m_paperBorderBottomInput->value(border);
}

void FlPosteRazorDialog::setPaperBorderLeft(double border)
{
	m_paperBorderLeftInput->value(border);
}

void FlPosteRazorDialog::setCustomPaperWidth(double width)
{
	m_paperCustomWidthInput->value(width);
}

void FlPosteRazorDialog::setCustomPaperHeight(double height)
{
	m_paperCustomHeightInput->value(height);
}

void FlPosteRazorDialog::setUseCustomPaperSize(bool useIt)
{
	// select the active tab
	m_paperFormatTypeTabs->value(useIt?m_paperFormatCustomGroup:m_paperFormatStandardGroup);
}

void FlPosteRazorDialog::setOverlappingWidth(double width)
{
	m_overlappingWidthInput->value(width);
}

void FlPosteRazorDialog::setOverlappingHeight(double height)
{
	m_overlappingHeightInput->value(height);
}

void FlPosteRazorDialog::setOverlappingPosition(PosteRazorEnums::eOverlappingPositions position)
{
	(
		position == PosteRazorEnums::eOverlappingPositionBottomRight?m_overlappingPositionBottomRightButton
		:position == PosteRazorEnums::eOverlappingPositionBottomLeft?m_overlappingPositionBottomLeftButton
		:position == PosteRazorEnums::eOverlappingPositionTopLeft?m_overlappingPositionTopLeftButton
		:m_overlappingPositionTopRightButton
	)->setonly();
}

void FlPosteRazorDialog::setPosterWidth(PosteRazorEnums::ePosterSizeModes mode, double width)
{
	if (mode == PosteRazorEnums::ePosterSizeModeAbsolute)
		m_posterAbsoluteWidthInput->value(width);
	else if (mode == PosteRazorEnums::ePosterSizeModePages)
		m_posterPagesWidthInput->value(width);
	else
		m_posterPercentualSizeInput->value(width);
}

void FlPosteRazorDialog::setPosterHeight(PosteRazorEnums::ePosterSizeModes mode, double height)
{
	if (mode == PosteRazorEnums::ePosterSizeModeAbsolute)
		m_posterAbsoluteHeightInput->value(height);
	else if (mode == PosteRazorEnums::ePosterSizeModePages)
		m_posterPagesHeightInput->value(height);
	else
		m_posterPercentualSizeInput->value(height);
}

void FlPosteRazorDialog::setPosterSizeMode(PosteRazorEnums::ePosterSizeModes mode)
{
	if (mode == PosteRazorEnums::ePosterSizeModeAbsolute)
		m_posterSizeAbsoluteRadioButton->setonly();
	else if (mode == PosteRazorEnums::ePosterSizeModePages)
		m_posterSizeInPagesRadioButton->setonly();
	else // if (mode == PosteRazorEnums::ePosterSizeModePercentual)
		m_posterSizePercentualRadioButton->setonly();
	updatePosterSizeGroupsState();
}

void FlPosteRazorDialog::setPosterHorizontalAlignment(PosteRazorEnums::eHorizontalAlignments alignment)
{
	(
		alignment == PosteRazorEnums::eHorizontalAlignmentLeft?m_posterAlignmentLeftButton
		:alignment == PosteRazorEnums::eHorizontalAlignmentCenter?m_posterAlignmentCenterButton
		:m_posterAlignmentRightButton 
	)->setonly();
}

void FlPosteRazorDialog::setPosterVerticalAlignment(PosteRazorEnums::eVerticalAlignments alignment)
{
	(
		alignment == PosteRazorEnums::eVerticalAlignmentTop?m_posterAlignmentTopButton
		:alignment == PosteRazorEnums::eVerticalAlignmentMiddle?m_posterAlignmentMiddleButton
		:m_posterAlignmentBottomButton 
	)->setonly();
}

void FlPosteRazorDialog::setPosterOutputFormat(ImageIOTypes::eImageFormats format)
{
}

void FlPosteRazorDialog::setLaunchPDFApplication(bool launch)
{
	m_setLaunchPDFApplicationCheckButton->value(launch?1:0);
}

void FlPosteRazorDialog::setPrevButtonEnabled(bool enabled)
{
	enabled?m_prevButton->activate():m_prevButton->deactivate();
}

void FlPosteRazorDialog::setNextButtonEnabled(bool enabled)
{
	enabled?m_nextButton->activate():m_nextButton->deactivate();
}

void FlPosteRazorDialog::setWizardStep(PosteRazorWizardDialogEnums::ePosteRazorWizardSteps step)
{
	m_wizard->value	(
		step == PosteRazorWizardDialogEnums::ePosteRazorWizardStepInputImage?m_loadInputImageStep
		:step == PosteRazorWizardDialogEnums::ePosteRazorWizardStepPaperSize?m_paperSizeStep
		:step == PosteRazorWizardDialogEnums::ePosteRazorWizardStepOverlapping?m_overlappingStep
		:step == PosteRazorWizardDialogEnums::ePosteRazorWizardStepPosterSize?m_posterSizeStep
		:/* step == PosteRazorWizardDialogEnums::ePosteRazorWizardStepSavePoster? */m_savePosterStep
	);

	updateStepInfoBar(step);
}

void FlPosteRazorDialog::setPreviewState(const char *state)
{
	m_paintCanvasGroup->setState(state);
	updatePreview();
}

#ifdef __APPLE__
static char OSX_droppedFilenameOnIcon[2048] = "";
static FlPosteRazorDialog *OSX_posteRazorDialogPointer = NULL;

static void	OSX_open_cb(const char* droppedFileName)
{
	if (OSX_posteRazorDialogPointer) {
		// File was dropped on the already running application
		OSX_posteRazorDialogPointer->loadInputImage(droppedFileName);
	} else {
		// File was dropped on an application icon, but PosteRazor was not yet running
		if (strcmp(OSX_droppedFilenameOnIcon, "") == 0) {
			// If more than one were dropped, use only the first one.
			strncpy(OSX_droppedFilenameOnIcon, droppedFileName, sizeof(OSX_droppedFilenameOnIcon));
			OSX_droppedFilenameOnIcon[sizeof(OSX_droppedFilenameOnIcon)-1] = '\0';
		}
	}
}
#endif

int main (int argc, char **argv)
{
#ifdef __APPLE__
	fl_open_callback(OSX_open_cb);
#endif

	Fl::get_system_colors();
	Fl::scheme("plastic");
	FlPosteRazorDialog dialog;

#ifdef WIN32
	dialog.icon((char *)LoadIcon(fl_display, MAKEINTRESOURCE(POSTERAZOR_ICON)));
#endif

	dialog.show();
	Fl::wait();

#if !defined(WIN32) && !defined(__APPLE__)
	XpmAttributes icon_attributes;
	Pixmap icon, icon_mask;
	icon_attributes.valuemask = XpmSize | XpmReturnPixels;
	XWMHints *hints;
	XpmCreatePixmapFromData(fl_display, DefaultRootWindow(fl_display), posteRazorXpm, &icon, &icon_mask, &icon_attributes);
	hints = XGetWMHints(fl_display, fl_xid(&dialog));
	hints->icon_pixmap = icon;
	hints->icon_mask = icon_mask;
	hints->flags = IconPixmapHint | IconMaskHint;
	XSetWMHints(fl_display, fl_xid(&dialog), hints);
#endif

#ifndef __APPLE__
	if (argc == 2)
		dialog.loadInputImage(argv[1]);
#else
	if (strcmp(OSX_droppedFilenameOnIcon, "") != 0)
		dialog.loadInputImage(OSX_droppedFilenameOnIcon);

	OSX_posteRazorDialogPointer = &dialog;
#endif

	return Fl::run();
}
/***************************************************************************
                          kdiff3.cpp  -  description
                             -------------------
    begin                : Don Jul 11 12:31:29 CEST 2002
    copyright            : (C) 2002-2004 by Joachim Eibl
    email                : joachim.eibl@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "diff.h"

#include <iostream>

// include files for QT
#include <qdir.h>
#include <qprinter.h>
#include <qpainter.h>
#include <qsplitter.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <qpopupmenu.h>
#include <qlabel.h>

// include files for KDE
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kmenubar.h>
#include <kstatusbar.h>
#include <klocale.h>
#include <kconfig.h>
#include <kstdaction.h>
#include <kcmdlineargs.h>
//#include <kkeydialog.h>

// application specific includes
#include "kdiff3.h"
#include "optiondialog.h"
#include "fileaccess.h"
#include "kdiff3_part.h"
#include "directorymergewindow.h"

#define ID_STATUS_MSG 1

KActionCollection* KDiff3App::actionCollection()
{
   if ( m_pKDiff3Shell==0 )
      return m_pKDiff3Part->actionCollection();
   else
      return m_pKDiff3Shell->actionCollection();
}

KStatusBar* KDiff3App::statusBar()
{
   if ( m_pKDiff3Shell==0 )
      return 0;
   else
      return m_pKDiff3Shell->statusBar();
}

KToolBar* KDiff3App::toolBar(const char* toolBarId )
{
   if ( m_pKDiff3Shell==0 )
      return 0;
   else
      return m_pKDiff3Shell->toolBar( toolBarId );
}

bool KDiff3App::isPart()
{
   return m_pKDiff3Shell==0;
}


KDiff3App::KDiff3App(QWidget* pParent, const char* name, KDiff3Part* pKDiff3Part )
   :QSplitter(pParent, name)  //previously KMainWindow
{
   m_pKDiff3Part = pKDiff3Part;
   m_pKDiff3Shell = dynamic_cast<KParts::MainWindow*>(pParent);

   setCaption( "KDiff3" );

   m_pMainSplitter = 0;
   m_pDirectoryMergeWindow = 0;
   m_pCornerWidget = 0;
   m_pMainWidget = 0;
   m_pDiffTextWindow1 = 0;
   m_pDiffTextWindow2 = 0;
   m_pDiffTextWindow3 = 0;
   m_pDiffWindowSplitter = 0;
   m_pOverview        = 0;
   m_bTripleDiff = false;
   m_pMergeResultWindow = 0;
   m_pMergeWindowFrame = 0;
   m_bOutputModified = false;
   m_bTimerBlock = false;

   // Needed before any file operations via FileAccess happen.
   g_pProgressDialog = new ProgressDialog(this);

   // All default values must be set before calling readOptions().
   m_pOptionDialog = new OptionDialog( m_pKDiff3Shell!=0, this );
   connect( m_pOptionDialog, SIGNAL(applyClicked()), this, SLOT(slotRefresh()) );
   
   m_pOptionDialog->readOptions( isPart() ? m_pKDiff3Part->instance()->config() : kapp->config() );

   m_sd1.setOptionDialog(m_pOptionDialog);
   m_sd2.setOptionDialog(m_pOptionDialog);
   m_sd3.setOptionDialog(m_pOptionDialog);
   
   // Option handling: Only when pParent==0 (no parent)
   KCmdLineArgs *args = isPart() ? 0 : KCmdLineArgs::parsedArgs();

   if (args!=0)
   {
      m_outputFilename = args->getOption("output");
      if ( m_outputFilename.isEmpty() )
         m_outputFilename = args->getOption("out");
   }

   m_bAuto = args!=0  && args->isSet("auto");
   if ( m_bAuto && m_outputFilename.isEmpty() )
   {
      //KMessageBox::information(this, i18n("Option --auto used, but no output file specified."));
      std::cerr << i18n("Option --auto used, but no output file specified.").ascii()<<std::endl;
      m_bAuto = false;
   }

   if ( m_outputFilename.isEmpty() && args!=0 && args->isSet("merge") )
   {
      m_outputFilename = "unnamed.txt";
      m_bDefaultFilename = true;
   }
   else
      m_bDefaultFilename = false;

   g_bAutoSolve = args!=0 && !args->isSet("qall");  // Note that this is effective only once.

   if ( args!=0 )
   {
      m_sd1.setFilename( args->getOption("base") );
      if ( m_sd1.isEmpty() )
      {
         if ( args->count() > 0 ) m_sd1.setFilename( args->url(0).url() ); // args->arg(0)
         if ( args->count() > 1 ) m_sd2.setFilename( args->url(1).url() );
         if ( args->count() > 2 ) m_sd3.setFilename( args->url(2).url() );
      }
      else
      {
         if ( args->count() > 0 ) m_sd2.setFilename( args->url(0).url() );
         if ( args->count() > 1 ) m_sd3.setFilename( args->url(1).url() );
      }

      QCStringList aliasList = args->getOptionList("fname");
      QCStringList::Iterator ali = aliasList.begin();
      if ( ali != aliasList.end() ) { m_sd1.setAliasName(*ali); ++ali; }
      if ( ali != aliasList.end() ) { m_sd2.setAliasName(*ali); ++ali; }
      if ( ali != aliasList.end() ) { m_sd3.setAliasName(*ali); ++ali; }
   }
   ///////////////////////////////////////////////////////////////////
   // call inits to invoke all other construction parts
   initActions(actionCollection());
   initStatusBar();

   m_pFindDialog = new FindDialog( this );
   connect( m_pFindDialog, SIGNAL(findNext()), this, SLOT(slotEditFindNext()));

   readOptions( isPart() ? m_pKDiff3Part->instance()->config() : kapp->config() );
   
   autoAdvance->setChecked( m_pOptionDialog->m_bAutoAdvance );
   showWhiteSpaceCharacters->setChecked( m_pOptionDialog->m_bShowWhiteSpaceCharacters );
   showWhiteSpace->setChecked( m_pOptionDialog->m_bShowWhiteSpace );
   showWhiteSpaceCharacters->setEnabled( m_pOptionDialog->m_bShowWhiteSpace );
   showLineNumbers->setChecked( m_pOptionDialog->m_bShowLineNumbers );

   m_pMainSplitter = this; //new QSplitter(this);
   m_pMainSplitter->setOrientation( Vertical );
//   setCentralWidget( m_pMainSplitter );
   m_pDirectoryMergeSplitter = new QSplitter( m_pMainSplitter );
   m_pDirectoryMergeSplitter->setOrientation( Horizontal );
   m_pDirectoryMergeWindow = new DirectoryMergeWindow( m_pDirectoryMergeSplitter, m_pOptionDialog,
      KApplication::kApplication()->iconLoader() );
   m_pDirectoryMergeInfo = new DirectoryMergeInfo( m_pDirectoryMergeSplitter );
   m_pDirectoryMergeWindow->setDirectoryMergeInfo( m_pDirectoryMergeInfo );
   connect( m_pDirectoryMergeWindow, SIGNAL(startDiffMerge(QString,QString,QString,QString,QString,QString,QString)),
            this, SLOT( slotFileOpen2(QString,QString,QString,QString,QString,QString,QString)));
   connect( m_pDirectoryMergeWindow, SIGNAL(selectionChanged()), this, SLOT(slotUpdateAvailabilities()));
   connect( m_pDirectoryMergeWindow, SIGNAL(currentChanged(QListViewItem*)), this, SLOT(slotUpdateAvailabilities()));
   connect( m_pDirectoryMergeWindow, SIGNAL(checkIfCanContinue(bool*)), this, SLOT(slotCheckIfCanContinue(bool*)));
   connect( m_pDirectoryMergeWindow, SIGNAL(updateAvailabilities()), this, SLOT(slotUpdateAvailabilities()));

   m_pDirectoryMergeWindow->initDirectoryMergeActions( this, actionCollection() );

   if ( args!=0 )  args->clear(); // Free up some memory.

   if (m_pKDiff3Shell==0)
   {
      completeInit();
   }
}


void KDiff3App::completeInit()
{
   if (m_pKDiff3Shell!=0)
   {
      QSize size=kapp->config()->readSizeEntry("Geometry");
      QPoint pos=kapp->config()->readPointEntry("Position");
      if(!size.isEmpty())
      {
         m_pKDiff3Shell->resize( size );
         m_pKDiff3Shell->move( pos );
      }
   }

   m_bDirCompare = improveFilenames();
   if ( m_bAuto && m_bDirCompare )
   {
      std::cerr << i18n("Option --auto ignored for directory comparison.").ascii()<<std::endl;
      m_bAuto = false;
   }
   if (!m_bDirCompare)
   {
      m_pDirectoryMergeSplitter->hide();

      init( m_bAuto );
      if ( m_bAuto )
      {
         SourceData* pSD=0;
         if ( m_sd3.isEmpty() )
         {
            if ( m_totalDiffStatus.bBinaryAEqB ){ pSD = &m_sd1; }
         }
         else
         {
            if      ( m_totalDiffStatus.bBinaryBEqC ){ pSD = &m_sd3; } // B==C (assume A is old)
            else if ( m_totalDiffStatus.bBinaryAEqB ){ pSD = &m_sd3; } // assuming C has changed
            else if ( m_totalDiffStatus.bBinaryAEqC ){ pSD = &m_sd2; } // assuming B has changed
         }

         if ( pSD!=0 )
         {
            // Save this file directly, not via the merge result window.
            bool bSuccess = false;
            FileAccess fa( m_outputFilename );
            if ( m_pOptionDialog->m_bDmCreateBakFiles && fa.exists() )
            {
               QString newName = m_outputFilename + ".orig";
               if ( FileAccess::exists( newName ) ) FileAccess::removeFile( newName );
               if ( !FileAccess::exists( newName ) ) fa.rename( newName );
            }
            
            bSuccess = pSD->saveNormalDataAs( m_outputFilename );
            if ( bSuccess ) ::exit(0);
            else KMessageBox::error( this, i18n("Saving failed.") );
         }
         else if ( m_pMergeResultWindow->getNrOfUnsolvedConflicts() == 0 )
         {
            bool bSuccess = m_pMergeResultWindow->saveDocument( m_outputFilename );
            if ( bSuccess ) ::exit(0);
         }
      }
   }

   if (statusBar() !=0 )
      statusBar()->setSizeGripEnabled(false);

   slotClipboardChanged(); // For initialisation.

   slotUpdateAvailabilities();

   if ( ! m_bDirCompare  &&  m_pKDiff3Shell!=0 )
   {
      bool bFileOpenError = false;
      if ( ! m_sd1.isEmpty() && !m_sd1.hasData()  ||
           ! m_sd2.isEmpty() && !m_sd2.hasData()  ||
           ! m_sd3.isEmpty() && !m_sd3.hasData() )
      {
         QString text( i18n("Opening of these files failed:") );
         text += "\n\n";
         if ( ! m_sd1.isEmpty() && !m_sd1.hasData() )
            text += " - " + m_sd1.getAliasName() + "\n";
         if ( ! m_sd2.isEmpty() && !m_sd2.hasData() )
            text += " - " + m_sd2.getAliasName() + "\n";
         if ( ! m_sd3.isEmpty() && !m_sd3.hasData() )
            text += " - " + m_sd3.getAliasName() + "\n";

         KMessageBox::sorry( this, text, i18n("File Open Error") );
         bFileOpenError = true;
      }

      if ( m_sd1.isEmpty() || m_sd2.isEmpty() || bFileOpenError )
         slotFileOpen();
   }
}

KDiff3App::~KDiff3App()
{

}

void KDiff3App::initActions( KActionCollection* ac )
{
   if (ac==0)   KMessageBox::error(0, "actionCollection==0");

   fileOpen = KStdAction::open(this, SLOT(slotFileOpen()), ac);
   fileOpen->setStatusText(i18n("Opens documents for comparison..."));
   fileSave = KStdAction::save(this, SLOT(slotFileSave()), ac);
   fileSave->setStatusText(i18n("Saves the merge result. All conflicts must be solved!"));
   fileSaveAs = KStdAction::saveAs(this, SLOT(slotFileSaveAs()), ac);
   fileSaveAs->setStatusText(i18n("Saves the current document as..."));
   fileQuit = KStdAction::quit(this, SLOT(slotFileQuit()), ac);
   fileQuit->setStatusText(i18n("Quits the application"));
   editCut = KStdAction::cut(this, SLOT(slotEditCut()), ac);
   editCut->setStatusText(i18n("Cuts the selected section and puts it to the clipboard"));
   editCopy = KStdAction::copy(this, SLOT(slotEditCopy()), ac);
   editCopy->setStatusText(i18n("Copies the selected section to the clipboard"));
   editPaste = KStdAction::paste(this, SLOT(slotEditPaste()), ac);
   editPaste->setStatusText(i18n("Pastes the clipboard contents to actual position"));
   editFind = KStdAction::find(this, SLOT(slotEditFind()), ac);
   editFind->setStatusText(i18n("Search for a string"));
   editFindNext = KStdAction::findNext(this, SLOT(slotEditFindNext()), ac);
   editFindNext->setStatusText(i18n("Search again for the string"));
   viewToolBar = KStdAction::showToolbar(this, SLOT(slotViewToolBar()), ac);
   viewToolBar->setStatusText(i18n("Enables/disables the toolbar"));
   viewStatusBar = KStdAction::showStatusbar(this, SLOT(slotViewStatusBar()), ac);
   viewStatusBar->setStatusText(i18n("Enables/disables the statusbar"));
   KStdAction::keyBindings(this, SLOT(slotConfigureKeys()), ac);
   KAction* pAction = KStdAction::preferences(this, SLOT(slotConfigure()), ac );
   if ( isPart() )
      pAction->setText(i18n("Configure KDiff3..."));


#include "xpm/downend.xpm"
#include "xpm/currentpos.xpm"
#include "xpm/down1arrow.xpm"
#include "xpm/down2arrow.xpm"
#include "xpm/upend.xpm"
#include "xpm/up1arrow.xpm"
#include "xpm/up2arrow.xpm"
#include "xpm/prevunsolved.xpm"
#include "xpm/nextunsolved.xpm"
#include "xpm/iconA.xpm"
#include "xpm/iconB.xpm"
#include "xpm/iconC.xpm"
#include "xpm/autoadvance.xpm"
#include "xpm/showwhitespace.xpm"
#include "xpm/showwhitespacechars.xpm"
#include "xpm/showlinenumbers.xpm"
//#include "reload.xpm"

   goCurrent = new KAction(i18n("Go to Current Delta"), QIconSet(QPixmap(currentpos)), CTRL+Key_Space, this, SLOT(slotGoCurrent()), ac, "go_current");
   goTop = new KAction(i18n("Go to First Delta"), QIconSet(QPixmap(upend)), 0, this, SLOT(slotGoTop()), ac, "go_top");
   goBottom = new KAction(i18n("Go to Last Delta"), QIconSet(QPixmap(downend)), 0, this, SLOT(slotGoBottom()), ac, "go_bottom");
   goPrevDelta = new KAction(i18n("Go to Previous Delta"), QIconSet(QPixmap(up1arrow)), CTRL+Key_Up, this, SLOT(slotGoPrevDelta()), ac, "go_prev_delta");
   goNextDelta = new KAction(i18n("Go to Next Delta"), QIconSet(QPixmap(down1arrow)), CTRL+Key_Down, this, SLOT(slotGoNextDelta()), ac, "go_next_delta");
   goPrevConflict = new KAction(i18n("Go to Previous Conflict"), QIconSet(QPixmap(up2arrow)), CTRL+Key_PageUp, this, SLOT(slotGoPrevConflict()), ac, "go_prev_conflict");
   goNextConflict = new KAction(i18n("Go to Next Conflict"), QIconSet(QPixmap(down2arrow)), CTRL+Key_PageDown, this, SLOT(slotGoNextConflict()), ac, "go_next_conflict");
   goPrevUnsolvedConflict = new KAction(i18n("Go to Previous Unsolved Conflict"), QIconSet(QPixmap(prevunsolved)), 0, this, SLOT(slotGoPrevUnsolvedConflict()), ac, "go_prev_unsolved_conflict");
   goNextUnsolvedConflict = new KAction(i18n("Go to Next Unsolved Conflict"), QIconSet(QPixmap(nextunsolved)), 0, this, SLOT(slotGoNextUnsolvedConflict()), ac, "go_next_unsolved_conflict");
   chooseA = new KToggleAction(i18n("Select Line(s) From A"), QIconSet(QPixmap(iconA)), CTRL+Key_1, this, SLOT(slotChooseA()), ac, "merge_choose_a");
   chooseB = new KToggleAction(i18n("Select Line(s) From B"), QIconSet(QPixmap(iconB)), CTRL+Key_2, this, SLOT(slotChooseB()), ac, "merge_choose_b");
   chooseC = new KToggleAction(i18n("Select Line(s) From C"), QIconSet(QPixmap(iconC)), CTRL+Key_3, this, SLOT(slotChooseC()), ac, "merge_choose_c");
   autoAdvance = new KToggleAction(i18n("Automatically Go to Next Unsolved Conflict After Source Selection"), QIconSet(QPixmap(autoadvance)), 0, this, SLOT(slotAutoAdvanceToggled()), ac, "merge_autoadvance");

   showWhiteSpaceCharacters = new KToggleAction(i18n("Show Space && Tabulator Characters for Differences"), QIconSet(QPixmap(showwhitespacechars)), 0, this, SLOT(slotShowWhiteSpaceToggled()), ac, "merge_show_whitespace_characters");
   showWhiteSpace = new KToggleAction(i18n("Show White Space"), QIconSet(QPixmap(showwhitespace)), 0, this, SLOT(slotShowWhiteSpaceToggled()), ac, "merge_show_whitespace");

   showLineNumbers = new KToggleAction(i18n("Show Line Numbers"), QIconSet(QPixmap(showlinenumbers)), 0, this, SLOT(slotShowLineNumbersToggled()), ac, "merge_showlinenumbers");
   chooseAEverywhere = new KAction(i18n("Choose A Everywhere"), CTRL+SHIFT+Key_1, this, SLOT(slotChooseAEverywhere()), ac, "merge_choose_a_everywhere");
   chooseBEverywhere = new KAction(i18n("Choose B Everywhere"), CTRL+SHIFT+Key_2, this, SLOT(slotChooseBEverywhere()), ac, "merge_choose_b_everywhere");
   chooseCEverywhere = new KAction(i18n("Choose C Everywhere"), CTRL+SHIFT+Key_3, this, SLOT(slotChooseCEverywhere()), ac, "merge_choose_c_everywhere");
   chooseAForUnsolvedConflicts = new KAction(i18n("Choose A For All Unsolved Conflicts"), 0, this, SLOT(slotChooseAForUnsolvedConflicts()), ac, "merge_choose_a_for_unsolved_conflicts");
   chooseBForUnsolvedConflicts = new KAction(i18n("Choose B For All Unsolved Conflicts"), 0, this, SLOT(slotChooseBForUnsolvedConflicts()), ac, "merge_choose_b_for_unsolved_conflicts");
   chooseCForUnsolvedConflicts = new KAction(i18n("Choose C For All Unsolved Conflicts"), 0, this, SLOT(slotChooseCForUnsolvedConflicts()), ac, "merge_choose_c_for_unsolved_conflicts");
   chooseAForUnsolvedWhiteSpaceConflicts = new KAction(i18n("Choose A For All Unsolved Whitespace Conflicts"), 0, this, SLOT(slotChooseAForUnsolvedWhiteSpaceConflicts()), ac, "merge_choose_a_for_unsolved_whitespace_conflicts");
   chooseBForUnsolvedWhiteSpaceConflicts = new KAction(i18n("Choose B For All Unsolved Whitespace Conflicts"), 0, this, SLOT(slotChooseBForUnsolvedWhiteSpaceConflicts()), ac, "merge_choose_b_for_unsolved_whitespace_conflicts");
   chooseCForUnsolvedWhiteSpaceConflicts = new KAction(i18n("Choose C For All Unsolved Whitespace Conflicts"), 0, this, SLOT(slotChooseCForUnsolvedWhiteSpaceConflicts()), ac, "merge_choose_c_for_unsolved_whitespace_conflicts");
   autoSolve = new KAction(i18n("Automatically Solve Simple Conflicts"), 0, this, SLOT(slotAutoSolve()), ac, "merge_autosolve");
   unsolve = new KAction(i18n("Set Deltas to Conflicts"), 0, this, SLOT(slotUnsolve()), actionCollection(), "merge_autounsolve");
   fileReload  = new KAction(i18n("Reload"), /*QIconSet(QPixmap(reloadIcon)),*/ Key_F5, this, SLOT(slotReload()), ac, "file_reload");
   showWindowA = new KToggleAction(i18n("Show Window A"), 0, this, SLOT(slotShowWindowAToggled()), ac, "win_show_a");
   showWindowB = new KToggleAction(i18n("Show Window B"), 0, this, SLOT(slotShowWindowBToggled()), ac, "win_show_b");
   showWindowC = new KToggleAction(i18n("Show Window C"), 0, this, SLOT(slotShowWindowCToggled()), ac, "win_show_c");
   winFocusNext = new KAction(i18n("Focus Next Window"), ALT+Key_Right, this, SLOT(slotWinFocusNext()), ac, "win_focus_next");
#ifdef _WIN32
   new KAction(i18n("Focus Next Window"), CTRL+Key_Tab, this, SLOT(slotWinFocusNext()), ac, "win_focus_next", false, false);
#endif
   winFocusPrev = new KAction(i18n("Focus Prev Window"), ALT+Key_Left, this, SLOT(slotWinFocusPrev()), ac, "win_focus_prev");
   winToggleSplitOrientation = new KAction(i18n("Toggle Split Orientation"), 0, this, SLOT(slotWinToggleSplitterOrientation()), ac, "win_toggle_split_orientation");

   dirShowBoth = new KToggleAction(i18n("Dir && Text Split Screen View"), 0, this, SLOT(slotDirShowBoth()), ac, "win_dir_show_both");
   dirShowBoth->setChecked( true );
   dirViewToggle = new KAction(i18n("Toggle Between Dir && Text View"), 0, this, SLOT(slotDirViewToggle()), actionCollection(), "win_dir_view_toggle");

   m_pMergeEditorPopupMenu = new QPopupMenu( this );
   chooseA->plug( m_pMergeEditorPopupMenu );
   chooseB->plug( m_pMergeEditorPopupMenu );
   chooseC->plug( m_pMergeEditorPopupMenu );
}


void KDiff3App::showPopupMenu( const QPoint& point )
{
   m_pMergeEditorPopupMenu->popup( point );
}

void KDiff3App::initStatusBar()
{
  ///////////////////////////////////////////////////////////////////
  // STATUSBAR
  if (statusBar() !=0 )
     statusBar()->message( i18n("Ready.") );
}

void KDiff3App::saveOptions( KConfig* config )
{
   if ( !isPart() )
   {
      config->setGroup("General Options");
      config->writeEntry("Geometry", m_pKDiff3Shell->size());
      config->writeEntry("Position", m_pKDiff3Shell->pos());
      config->writeEntry("Show Toolbar", viewToolBar->isChecked());
      config->writeEntry("Show Statusbar",viewStatusBar->isChecked());
      if(toolBar("mainToolBar")!=0)
         config->writeEntry("ToolBarPos", (int) toolBar("mainToolBar")->barPos());
   }

   m_pOptionDialog->saveOptions( config );
}


void KDiff3App::readOptions( KConfig* config )
{
   if( !isPart() )
   {
      config->setGroup("General Options");

      // bar status settings
      bool bViewToolbar = config->readBoolEntry("Show Toolbar", true);
      viewToolBar->setChecked(bViewToolbar);
      slotViewToolBar();

      bool bViewStatusbar = config->readBoolEntry("Show Statusbar", true);
      viewStatusBar->setChecked(bViewStatusbar);
      slotViewStatusBar();


      // bar position settings
      KToolBar::BarPosition toolBarPos;
      toolBarPos=(KToolBar::BarPosition) config->readNumEntry("ToolBarPos", KToolBar::Top);
      if( toolBar("mainToolBar")!=0 )
         toolBar("mainToolBar")->setBarPos(toolBarPos);

      QSize size=config->readSizeEntry("Geometry");
      QPoint pos=config->readPointEntry("Position");
      if(!size.isEmpty())
      {
         m_pKDiff3Shell->resize( size );
         m_pKDiff3Shell->move( pos );
      }
   }

   slotRefresh();
}


bool KDiff3App::queryClose()
{
   saveOptions( isPart() ? m_pKDiff3Part->instance()->config() : kapp->config() );

   if(m_bOutputModified)
   {
      int result = KMessageBox::warningYesNoCancel(this,
         i18n("The merge result hasn't been saved."),
         i18n("Warning"), i18n("Save && Quit"), i18n("Quit Without Saving") );
      if ( result==KMessageBox::Cancel )
         return false;
      else if ( result==KMessageBox::Yes )
      {
         slotFileSave();
         if ( m_bOutputModified )
         {
            KMessageBox::sorry(this, i18n("Saving the merge result failed."), i18n("Warning") );
            return false;
         }
      }
   }

   m_bOutputModified = false;

   if ( m_pDirectoryMergeWindow->isDirectoryMergeInProgress() )
   {
      int result = KMessageBox::warningYesNo(this,
         i18n("You are currently doing a directory merge. Are you sure, you want to abort?"),
         i18n("Warning"), i18n("Quit"), i18n("Continue Merging") );
      if ( result!=KMessageBox::Yes )
         return false;
   }

   return true;
}


/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////


void KDiff3App::slotFileSave()
{
   if ( m_bDefaultFilename )
   {
      slotFileSaveAs();
   }
   else
   {
      slotStatusMsg(i18n("Saving file..."));

      bool bSuccess = m_pMergeResultWindow->saveDocument( m_outputFilename );
      if ( bSuccess )
      {
         m_bOutputModified = false;
         if ( m_bDirCompare )
            m_pDirectoryMergeWindow->mergeResultSaved(m_outputFilename);
      }

      slotStatusMsg(i18n("Ready."));
   }
}

void KDiff3App::slotFileSaveAs()
{
  slotStatusMsg(i18n("Saving file with a new filename..."));

  QString s = KFileDialog::getSaveURL( QDir::currentDirPath(), 0, this, i18n("Save As...") ).url();
  if(!s.isEmpty())
  {
     m_outputFilename = s;
     bool bSuccess = m_pMergeResultWindow->saveDocument( m_outputFilename );
     if ( bSuccess )
     {
        m_bOutputModified = false;
        if ( m_bDirCompare )
           m_pDirectoryMergeWindow->mergeResultSaved(m_outputFilename);
     }
     //setCaption(url.fileName(),doc->isModified());

     m_bDefaultFilename = false;
  }

  slotStatusMsg(i18n("Ready."));
}


void KDiff3App::slotFileQuit()
{
   slotStatusMsg(i18n("Exiting..."));

   if( !queryClose() )
       return;      // Don't quit

   KApplication::exit(0);
}



void KDiff3App::slotViewToolBar()
{
   slotStatusMsg(i18n("Toggling toolbar..."));
   ///////////////////////////////////////////////////////////////////
   // turn Toolbar on or off
   if ( toolBar("mainToolBar") !=0 )
   {
      if(!viewToolBar->isChecked())
      {
         toolBar("mainToolBar")->hide();
      }
      else
      {
         toolBar("mainToolBar")->show();
      }
   }

   slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotViewStatusBar()
{
   slotStatusMsg(i18n("Toggle the statusbar..."));
   ///////////////////////////////////////////////////////////////////
   //turn Statusbar on or off
   if (statusBar() !=0 )
   {
      if(!viewStatusBar->isChecked())
      {
         statusBar()->hide();
      }
      else
      {
         statusBar()->show();
      }
   }

   slotStatusMsg(i18n("Ready."));
}


void KDiff3App::slotStatusMsg(const QString &text)
{
   ///////////////////////////////////////////////////////////////////
   // change status message permanently
   if (statusBar() !=0 )
   {
      statusBar()->clear();
      statusBar()->message( text );
   }
}



FindDialog::FindDialog(QWidget* pParent)
: QDialog( pParent )
{
   QGridLayout* layout = new QGridLayout( this );
   layout->setMargin(5);
   layout->setSpacing(5);

   int line=0;
   layout->addMultiCellWidget( new QLabel(i18n("Searchtext:"),this), line,line,0,1 );
   ++line;

   m_pSearchString = new QLineEdit( this );
   layout->addMultiCellWidget( m_pSearchString, line,line,0,1 );
   ++line;

   m_pCaseSensitive = new QCheckBox(i18n("Case sensitive"),this);
   layout->addWidget( m_pCaseSensitive, line, 1 );

   m_pSearchInA = new QCheckBox(i18n("Search A"),this);
   layout->addWidget( m_pSearchInA, line, 0 );
   m_pSearchInA->setChecked( true );
   ++line;

   m_pSearchInB = new QCheckBox(i18n("Search B"),this);
   layout->addWidget( m_pSearchInB, line, 0 );
   m_pSearchInB->setChecked( true );
   ++line;

   m_pSearchInC = new QCheckBox(i18n("Search C"),this);
   layout->addWidget( m_pSearchInC, line, 0 );
   m_pSearchInC->setChecked( true );
   ++line;

   m_pSearchInOutput = new QCheckBox(i18n("Search output"),this);
   layout->addWidget( m_pSearchInOutput, line, 0 );
   m_pSearchInOutput->setChecked( true );
   ++line;

   QPushButton* pButton = new QPushButton( i18n("&Search"), this );
   layout->addWidget( pButton, line, 0 );
   connect( pButton, SIGNAL(clicked()), this, SLOT(accept()));

   pButton = new QPushButton( i18n("&Cancel"), this );
   layout->addWidget( pButton, line, 1 );
   connect( pButton, SIGNAL(clicked()), this, SLOT(reject()));

   hide();
}

#include "kdiff3.moc"

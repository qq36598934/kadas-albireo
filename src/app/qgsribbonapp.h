/***************************************************************************
      qgsribbonapp.h - QGIS Ribbon Interface
      --------------------------------------
    begin                : Wed Dec 16 2015
    copyright            : (C) 2015 by Sandro Mani
    email                : smani at sourcepole dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSRIBBONAPP_H
#define QGSRIBBONAPP_H

#include "qgisapp.h"
#include "ui_qgsribbonwindowbase.h"
#include "ui_qgsribbontopwidget.h"
#include "ui_qgsribbonstatuswidget.h"

class QgsCoordinateDisplayer;

class QgsRibbonApp: public QgisApp, private Ui::QgsRibbonWindowBase, private Ui::QgsRibbonTopWidget, private Ui::QgsRibbonStatusWidget
{
    Q_OBJECT
  public:

    static QgsRibbonApp* instance() { return qobject_cast<QgsRibbonApp*>( smInstance ); }

    QgsRibbonApp( QSplashScreen *splash, bool restorePlugins = true, QWidget *parent = 0, Qt::WindowFlags fl = Qt::Window );
    QgsRibbonApp();
    ~QgsRibbonApp();

    QgsMapCanvas *mapCanvas() const override { return mMapCanvas; }
    QgsLayerTreeView* layerTreeView() const override { return mLayerTreeView; }
    QgsMessageBar *messageBar() const override { return mInfoBar; }
    QMenu *projectMenu() const override { return mProjectMenu; }
    QMenu *editMenu() const override { return mEditMenu; }
    QMenu *viewMenu() const override { return mViewMenu; }
    QMenu *layerMenu() const override { return mLayerMenu; }
    QMenu *newLayerMenu() const override { return mNewLayerMenu; }
    //! @note added in 2.5
    QMenu *addLayerMenu() const override { return mAddLayerMenu; }
    QMenu *settingsMenu() const override { return mSettingsMenu; }
    QMenu *pluginMenu() const override { return mPluginMenu; }
    QMenu *databaseMenu() const override { return mDatabaseMenu; }
    QMenu *rasterMenu() const override { return mRasterMenu; }
    QMenu *vectorMenu() const override { return mVectorMenu; }
    QMenu *webMenu() const override { return mWebMenu; }
#ifdef Q_OS_MAC
    QMenu *firstRightStandardMenu() const override { return mWindowMenu; }
    QMenu *windowMenu() const override { return mWindowMenu; }
#else
    QMenu *firstRightStandardMenu() const override { return mHelpMenu; }
    QMenu *windowMenu() const override { return NULL; }
#endif
    QMenu *helpMenu() const override { return mHelpMenu; }
    QMenu *recentProjectsMenu() const override { return mRecentProjectsMenu; }
    QMenu *projectFromTemplateMenu() const override { return mProjectTemplatesMenu; }
    QMenu *printComposersMenu() const override { return mPrintComposersMenu; }
    QMenu *panelMenu() const override { return mPanelMenu; }
    QMenu *featureActionMenu() const override { return mFeatureActionMenu; }
    void setTheme( QString themeName ) override;
    void getCoordinateDisplayFormat( QgsCoordinateUtils::TargetFormat& format, QString& epsg ) override;

  private slots:
    void activateDeactivateLayerRelatedActions( QgsMapLayer */*layer*/ ) override {}
    void updateLayerModifiedActions() override {}
    void on_mLayerTreeViewButton_clicked();
    void on_mZoomInButton_clicked();
    void on_mZoomOutButton_clicked();
    void checkOnTheFlyProjection( const QList<QgsMapLayer*>& newLayers );
    void addCameraPicture();
    void userScale();
    void showScale( double scale );

  private:
    QMenu* mProjectMenu;
    QMenu* mEditMenu;
    QMenu* mViewMenu;
    QMenu* mLayerMenu;
    QMenu* mNewLayerMenu;
    QMenu* mAddLayerMenu;
    QMenu* mSettingsMenu;
    QMenu* mPluginMenu;
    QMenu* mDatabaseMenu;
    QMenu* mRasterMenu;
    QMenu* mVectorMenu;
    QMenu* mWebMenu;
    QMenu* mWindowMenu;
    QMenu* mHelpMenu;
    QMenu* mPrintComposersMenu;
    QMenu* mRecentProjectsMenu;
    QMenu* mProjectTemplatesMenu;
    QMenu* mPanelMenu;
    QMenu* mFeatureActionMenu;
    QgsCoordinateDisplayer* mCoordinateDisplayer;
    QgsMessageBar* mInfoBar;

    QPoint mDragStartPos;
    QString mDragStartActionName;
    QPointer<QgsMessageBarItem> mReprojMsgItem;

    bool eventFilter( QObject *obj, QEvent *ev ) override;
    void resizeEvent( QResizeEvent *event ) override;
    void mousePressEvent( QMouseEvent* event ) override;
    void mouseMoveEvent( QMouseEvent* event ) override;
    void dropEvent( QDropEvent* event ) override;
    void dragEnterEvent( QDragEnterEvent* event ) override;
    void performDrag( const QIcon* icon );
    void restoreFavoriteButton( QToolButton* button );
    void configureButtons();
    void setActionToButton( QAction* action, QToolButton* button , QgsMapTool *tool = 0 );
    void updateWidgetPositions();
    void initLayerTreeView();
};

#endif // QGSRIBBONAPP_H
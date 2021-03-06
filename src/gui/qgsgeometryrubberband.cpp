/***************************************************************************
                         qgsgeometryrubberband.cpp
                         -------------------------
    begin                : December 2014
    copyright            : (C) 2014 by Marco Hugentobler
    email                : marco at sourcepole dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsgeometryrubberband.h"
#include "qgsgeometry.h"
#include "qgsgeometryengine.h"
#include "qgsabstractgeometryv2.h"
#include "qgsmapcanvas.h"
#include "qgspointv2.h"
#include "qgscompoundcurvev2.h"
#include "qgscurvev2.h"
#include "qgspolygonv2.h"
#include "qgslinestringv2.h"
#include "qgscircularstringv2.h"
#include "qgsgeometrycollectionv2.h"
#include "qgsgeometryutils.h"
#include "qgsproject.h"
#include <QPainter>

QgsGeometryRubberBand::QgsGeometryRubberBand( QgsMapCanvas* mapCanvas, QGis::GeometryType geomType )
    : QgsMapCanvasItem( mapCanvas )
    , mGeometry( 0 )
    , mPen( Qt::red )
    , mBrush( Qt::red )
    , mIconSize( 5 )
    , mIconType( ICON_BOX )
    , mIconPen( Qt::black, 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin )
    , mIconBrush( Qt::transparent )
    , mGeometryType( geomType )
    , mMeasurementMode( MEASURE_NONE )
    , mMeasurer( 0 )
{
  mTranslationOffset[0] = 0.;
  mTranslationOffset[1] = 0.;

  connect( mapCanvas, SIGNAL( mapCanvasRefreshed() ), this, SLOT( redrawMeasurements() ) );
  connect( mapCanvas, SIGNAL( destinationCrsChanged() ), this, SLOT( configureDistanceArea() ) );
  connect( mapCanvas, SIGNAL( hasCrsTransformEnabledChanged( bool ) ), this, SLOT( configureDistanceArea() ) );
  connect( QgsProject::instance(), SIGNAL( readProject( QDomDocument ) ), this, SLOT( configureDistanceArea() ) );
  configureDistanceArea();
}

QgsGeometryRubberBand::~QgsGeometryRubberBand()
{
  qDeleteAll( mMeasurementLabels );
  delete mGeometry;
  delete mMeasurer;
}

void QgsGeometryRubberBand::updatePosition()
{
  setRect( rubberBandRectangle() );
  QgsMapCanvasItem::updatePosition();
}

void QgsGeometryRubberBand::paint( QPainter* painter )
{
  if ( !mGeometry || !painter )
  {
    return;
  }

  painter->save();
  painter->translate( -pos() );

  if ( mGeometryType == QGis::Polygon )
  {
    painter->setBrush( mBrush );
  }
  else
  {
    painter->setBrush( Qt::NoBrush );
  }
  painter->setPen( mPen );


  QgsAbstractGeometryV2* paintGeom = mGeometry->clone();

  QTransform mapToCanvas = mMapCanvas->getCoordinateTransform()->transform();
  QTransform translationOffset = QTransform::fromTranslate( mTranslationOffset[0], mTranslationOffset[1] );

  paintGeom->transform( translationOffset * mapToCanvas );
  paintGeom->draw( *painter );

  //draw vertices
  QgsVertexId vertexId;
  QgsPointV2 vertex;
  while ( paintGeom->nextVertex( vertexId, vertex ) )
  {
    if ( !mHiddenNodes.contains( vertexId ) )
    {
      drawVertex( painter, vertex.x(), vertex.y() );
    }
  }

  delete paintGeom;
  painter->restore();
}

void QgsGeometryRubberBand::drawVertex( QPainter* p, double x, double y )
{
  qreal s = ( mIconSize - 1 ) / 2;
  p->save();
  p->setPen( mIconPen );
  p->setBrush( mIconBrush );

  switch ( mIconType )
  {
    case ICON_NONE:
      break;

    case ICON_CROSS:
      p->drawLine( QLineF( x - s, y, x + s, y ) );
      p->drawLine( QLineF( x, y - s, x, y + s ) );
      break;

    case ICON_X:
      p->drawLine( QLineF( x - s, y - s, x + s, y + s ) );
      p->drawLine( QLineF( x - s, y + s, x + s, y - s ) );
      break;

    case ICON_BOX:
      p->drawLine( QLineF( x - s, y - s, x + s, y - s ) );
      p->drawLine( QLineF( x + s, y - s, x + s, y + s ) );
      p->drawLine( QLineF( x + s, y + s, x - s, y + s ) );
      p->drawLine( QLineF( x - s, y + s, x - s, y - s ) );
      break;

    case ICON_FULL_BOX:
      p->drawRect( x - s, y - s, mIconSize, mIconSize );
      break;

    case ICON_CIRCLE:
      p->drawEllipse( x - s, y - s, mIconSize, mIconSize );
      break;

    case ICON_TRIANGLE:
      p->drawLine( QLineF( x - s, y + s, x + s, y + s ) );
      p->drawLine( QLineF( x + s, y + s, x, y - s ) );
      p->drawLine( QLineF( x, y - s, x - s, y + s ) );
      break;

    case ICON_FULL_TRIANGLE:
      p->drawPolygon( QPolygonF() <<
                      QPointF( x - s, y + s ) <<
                      QPointF( x + s, y + s ) <<
                      QPointF( x, y - s ) );
      break;
  }
  p->restore();
}

void QgsGeometryRubberBand::configureDistanceArea()
{
  mDa.setEllipsoid( QgsProject::instance()->readEntry( "Measure", "/Ellipsoid", GEO_NONE ) );
  mDa.setEllipsoidalMode( mMapCanvas->mapSettings().hasCrsTransformEnabled() );
  mDa.setSourceCrs( mMapCanvas->mapSettings().destinationCrs() );
}

void QgsGeometryRubberBand::redrawMeasurements()
{
  qDeleteAll( mMeasurementLabels );
  mMeasurementLabels.clear();
  mPartMeasurements.clear();
  if ( mGeometry )
  {
    if ( mMeasurementMode != MEASURE_NONE )
    {
      if ( dynamic_cast<QgsGeometryCollectionV2*>( mGeometry ) )
      {
        QgsGeometryCollectionV2* collection = static_cast<QgsGeometryCollectionV2*>( mGeometry );
        for ( int i = 0, n = collection->numGeometries(); i < n; ++i )
        {
          measureGeometry( collection->geometryN( i ), i );
        }
      }
      else
      {
        measureGeometry( mGeometry, 0 );
      }
    }
  }
}

void QgsGeometryRubberBand::setGeometry( QgsAbstractGeometryV2* geom , const QList<QgsVertexId> &hiddenNodes )
{
  mHiddenNodes.clear();
  delete mGeometry;
  mGeometry = geom;
  qDeleteAll( mMeasurementLabels );
  mMeasurementLabels.clear();
  mPartMeasurements.clear();

  if ( !mGeometry )
  {
    setRect( QgsRectangle() );
    return;
  }
  mHiddenNodes = hiddenNodes;

  setRect( rubberBandRectangle() );

  if ( mMeasurementMode != MEASURE_NONE )
  {
    if ( dynamic_cast<QgsGeometryCollectionV2*>( mGeometry ) )
    {
      QgsGeometryCollectionV2* collection = static_cast<QgsGeometryCollectionV2*>( mGeometry );
      for ( int i = 0, n = collection->numGeometries(); i < n; ++i )
      {
        measureGeometry( collection->geometryN( i ), i );
      }
    }
    else
    {
      measureGeometry( mGeometry, 0 );
    }
  }
}

bool QgsGeometryRubberBand::contains( const QgsPoint& p, double tol ) const
{
  if ( !mGeometry )
  {
    return false;
  }

  QgsPolygonV2 filterRect;
  QgsLineStringV2* exterior = new QgsLineStringV2();
  exterior->setPoints( QList<QgsPointV2>()
                       << QgsPointV2( p.x() - tol, p.y() - tol )
                       << QgsPointV2( p.x() + tol, p.y() - tol )
                       << QgsPointV2( p.x() + tol, p.y() + tol )
                       << QgsPointV2( p.x() - tol, p.y() + tol )
                       << QgsPointV2( p.x() - tol, p.y() - tol ) );
  filterRect.setExteriorRing( exterior );

  QgsGeometryEngine* geomEngine = QgsGeometry::createGeometryEngine( mGeometry );
  bool intersects = geomEngine->intersects( filterRect );
  delete geomEngine;
  return intersects;
}

void QgsGeometryRubberBand::setTranslationOffset( double dx, double dy )
{
  mTranslationOffset[0] = dx;
  mTranslationOffset[1] = dy;
  if ( mGeometry )
  {
    setRect( rubberBandRectangle() );
  }
}

void QgsGeometryRubberBand::moveVertex( const QgsVertexId& id, const QgsPointV2& newPos )
{
  if ( mGeometry )
  {
    mGeometry->moveVertex( id, newPos );
    setRect( rubberBandRectangle() );
  }
}

void QgsGeometryRubberBand::setFillColor( const QColor& c )
{
  mBrush.setColor( c );
}

QColor QgsGeometryRubberBand::fillColor() const
{
  return mBrush.color();
}

void QgsGeometryRubberBand::setOutlineColor( const QColor& c )
{
  mPen.setColor( c );
}

QColor QgsGeometryRubberBand::outlineColor() const
{
  return mPen.color();
}

void QgsGeometryRubberBand::setOutlineWidth( int width )
{
  mPen.setWidth( width );
  setRect( rubberBandRectangle() );
}

int QgsGeometryRubberBand::outlineWidth() const
{
  return mPen.width();
}

void QgsGeometryRubberBand::setLineStyle( Qt::PenStyle penStyle )
{
  mPen.setStyle( penStyle );
}

Qt::PenStyle QgsGeometryRubberBand::lineStyle() const
{
  return mPen.style();
}

void QgsGeometryRubberBand::setBrushStyle( Qt::BrushStyle brushStyle )
{
  mBrush.setStyle( brushStyle );
}

Qt::BrushStyle QgsGeometryRubberBand::brushStyle() const
{
  return mBrush.style();
}

void QgsGeometryRubberBand::setIconSize( int iconSize )
{
  mIconSize = iconSize;
  setRect( rubberBandRectangle() );
}

void QgsGeometryRubberBand::setIconFillColor( const QColor& c )
{
  mIconBrush.setColor( c );
}

void QgsGeometryRubberBand::setIconOutlineColor( const QColor& c )
{
  mIconPen.setColor( c );
}

void QgsGeometryRubberBand::setIconOutlineWidth( int width )
{
  mIconPen.setWidth( width );
}

void QgsGeometryRubberBand::setIconLineStyle( Qt::PenStyle penStyle )
{
  mIconPen.setStyle( penStyle );
}

void QgsGeometryRubberBand::setIconBrushStyle( Qt::BrushStyle brushStyle )
{
  mIconBrush.setStyle( brushStyle );
}

void QgsGeometryRubberBand::setMeasurementMode( MeasurementMode measurementMode, QGis::UnitType displayUnits , AngleUnit angleUnit , AzimuthNorth azimuthNorth )
{
  mMeasurementMode = measurementMode;
  mDisplayUnits = displayUnits;
  mAngleUnit = angleUnit;
  mAzimuthNorth = azimuthNorth;
  redrawMeasurements();
}

QString QgsGeometryRubberBand::getTotalMeasurement() const
{
  if ( mMeasurementMode == MEASURE_ANGLE || mMeasurementMode == MEASURE_AZIMUTH )
  {
    return ""; /* Does not make sense for angles */
  }
  double total = 0;
  foreach ( double value, mPartMeasurements )
  {
    total += value;
  }
  if ( mMeasurementMode == MEASURE_LINE || mMeasurementMode == MEASURE_LINE_AND_SEGMENTS )
  {
    return formatMeasurement( total, false );
  }
  else
  {
    return formatMeasurement( total, true );
  }
}

QgsRectangle QgsGeometryRubberBand::rubberBandRectangle() const
{
  if ( !mGeometry )
  {
    return QgsRectangle();
  }
  qreal scale = mMapCanvas->mapUnitsPerPixel();
  qreal s = ( mIconSize - 1 ) / 2.0 * scale;
  qreal p = mPen.width() * scale;
  QgsRectangle rect = mGeometry->boundingBox().buffer( s + p );
  rect.setXMinimum( rect.xMinimum() + mTranslationOffset[0] );
  rect.setYMinimum( rect.yMinimum() + mTranslationOffset[1] );
  rect.setXMaximum( rect.xMaximum() + mTranslationOffset[0] );
  rect.setYMaximum( rect.yMaximum() + mTranslationOffset[1] );
  return rect;
}

void QgsGeometryRubberBand::measureGeometry( QgsAbstractGeometryV2 *geometry, int part )
{
  QStringList measurements;
  if ( mMeasurer )
  {
    foreach ( const Measurer::Measurement& measurement, mMeasurer->measure( mMeasurementMode, part, geometry, mPartMeasurements ) )
    {
      QString value = "";
      if ( measurement.type == Measurer::Measurement::Length )
      {
        value = formatMeasurement( measurement.value, false );
      }
      else if ( measurement.type == Measurer::Measurement::Area )
      {
        value = formatMeasurement( measurement.value, true );
      }
      else if ( measurement.type == Measurer::Measurement::Angle )
      {
        value = formatAngle( measurement.value );
      }
      if ( !measurement.label.isEmpty() )
      {
        value = QString( "%1: %2" ).arg( measurement.label ).arg( value );
      }
      measurements.append( value );
    }
  }
  else
  {
    switch ( mMeasurementMode )
    {
      case MEASURE_LINE:
        if ( dynamic_cast<QgsCurveV2*>( geometry ) )
        {
          double length = mDa.measureLine( static_cast<QgsCurveV2*>( geometry ) );
          mPartMeasurements.append( length );
          measurements.append( formatMeasurement( length, false ) );
        }
        break;
      case MEASURE_LINE_AND_SEGMENTS:
        if ( dynamic_cast<QgsCurveV2*>( geometry ) )
        {
          QgsVertexId vid;
          QgsPointV2 p;
          QList<QgsPointV2> points;
          while ( geometry->nextVertex( vid, p ) )
          {
            if ( !mHiddenNodes.contains( vid ) )
            {
              points.append( p );
            }
          }
          double totLength = 0;
          for ( int i = 0, n = points.size() - 1; i < n; ++i )
          {
            QgsPoint p1( points[i].x(), points[i].y() );
            QgsPoint p2( points[i+1].x(), points[i+1].y() );
            double segmentLength = mDa.measureLine( p1, p2 );
            totLength += segmentLength;
            if ( n > 1 )
            {
              addMeasurements( QStringList() << formatMeasurement( segmentLength, false ), QgsPointV2( 0.5 * ( p1.x() + p2.x() ), 0.5 * ( p1.y() + p2.y() ) ) );
            }
          }
          if ( !points.isEmpty() )
          {
            mPartMeasurements.append( totLength );
            QString totLengthStr = QApplication::translate( "QgsGeometryRubberBand", "Tot.: %1" ).arg( formatMeasurement( totLength, false ) );
            addMeasurements( QStringList() << totLengthStr, points.last() );
          }
        }
        break;
      case MEASURE_AZIMUTH:
        if ( dynamic_cast<QgsCurveV2*>( geometry ) )
        {
          QgsVertexId vid;
          QgsPointV2 p;
          QList<QgsPointV2> points;
          while ( geometry->nextVertex( vid, p ) )
          {
            if ( !mHiddenNodes.contains( vid ) )
            {
              points.append( p );
            }
          }
          for ( int i = 0, n = points.size() - 1; i < n; ++i )
          {
            QgsPoint p1( points[i].x(), points[i].y() );
            QgsPoint p2( points[i+1].x(), points[i+1].y() );

            double angle = 0;
            if ( mAzimuthNorth == AZIMUTH_NORTH_GEOGRAPHIC )
            {
              angle = mDa.bearing( p1, p2 );
            }
            else
            {
              angle = qAtan2( p2.x() - p1.x(), p2.y() - p1.y() );
            }
            angle = qRound( angle *  1000 ) / 1000.;
            angle = angle < 0 ? angle + 2 * M_PI : angle;
            angle = angle >= 2 * M_PI ? angle - 2 * M_PI : angle;
            mPartMeasurements.append( angle );
            QString segmentLength = formatAngle( angle );
            addMeasurements( QStringList() << segmentLength, QgsPointV2( 0.5 * ( p1.x() + p2.x() ), 0.5 * ( p1.y() + p2.y() ) ) );
          }
        }
        break;
      case MEASURE_ANGLE:
        // Note: only works with circular sector geometry
        if ( dynamic_cast<QgsCurvePolygonV2*>( geometry ) && dynamic_cast<QgsCompoundCurveV2*>( static_cast<QgsCurvePolygonV2*>( geometry )->exteriorRing() ) )
        {
          QgsCompoundCurveV2* curve = static_cast<QgsCompoundCurveV2*>( static_cast<QgsCurvePolygonV2*>( geometry )->exteriorRing() );
          if ( !curve->isEmpty() )
          {
            if ( dynamic_cast<const QgsCircularStringV2*>( curve->curveAt( 0 ) ) )
            {
              const QgsCircularStringV2* circularString = static_cast<const QgsCircularStringV2*>( curve->curveAt( 0 ) );
              if ( circularString->vertexCount() == 3 )
              {
                QgsPointV2 p1 = circularString->pointN( 0 );
                QgsPointV2 p2 = circularString->pointN( 1 );
                QgsPointV2 p3 = circularString->pointN( 2 );
                double angle;
                if ( p1 == p3 )
                {
                  angle = 2 * M_PI;
                }
                else
                {
                  double radius, cx, cy;
                  QgsGeometryUtils::circleCenterRadius( p1, p2, p3, radius, cx, cy );

                  double azimuthOne = mDa.bearing( QgsPoint( cx, cy ), QgsPoint( p1.x(), p1.y() ) );
                  double azimuthTwo = mDa.bearing( QgsPoint( cx, cy ), QgsPoint( p3.x(), p3.y() ) );
                  azimuthOne = azimuthOne < 0 ? azimuthOne + 2 * M_PI : azimuthOne;
                  azimuthTwo = azimuthTwo < 0 ? azimuthTwo + 2 * M_PI : azimuthTwo;
                  azimuthTwo = azimuthTwo < azimuthOne ? azimuthTwo + 2 * M_PI : azimuthTwo;
                  angle = azimuthTwo - azimuthOne;
                }
                mPartMeasurements.append( angle );
                QString segmentLength = formatAngle( angle );
                addMeasurements( QStringList() << segmentLength, p2 );
              }
            }
            else if ( dynamic_cast<const QgsLineStringV2*>( curve->curveAt( 0 ) ) )
            {
              const QgsLineStringV2* lineString = static_cast<const QgsLineStringV2*>( curve->curveAt( 0 ) );
              if ( lineString->vertexCount() == 3 )
              {
                mPartMeasurements.append( 0 );
                addMeasurements( QStringList() << formatAngle( 0 ), lineString->pointN( 1 ) );
              }
            }
          }
        }
        break;
      case MEASURE_POLYGON:
        if ( dynamic_cast<QgsCurvePolygonV2*>( geometry ) )
        {
          double area = mDa.measurePolygon( static_cast<QgsCurvePolygonV2*>( geometry )->exteriorRing() );
          mPartMeasurements.append( area );
          measurements.append( formatMeasurement( area, true ) );
        }
        break;
      case MEASURE_RECTANGLE:
        if ( dynamic_cast<QgsPolygonV2*>( geometry ) )
        {
          double area = mDa.measurePolygon( static_cast<QgsCurvePolygonV2*>( geometry )->exteriorRing() );
          mPartMeasurements.append( area );
          measurements.append( formatMeasurement( area, true ) );
          QgsCurveV2* ring = static_cast<QgsPolygonV2*>( geometry )->exteriorRing();
          if ( ring->vertexCount() >= 4 )
          {
            QList<QgsPointV2> points;
            ring->points( points );
            QgsPoint p1( points[0].x(), points[0].y() );
            QgsPoint p2( points[2].x(), points[2].y() );
            QString width = formatMeasurement( mDa.measureLine( p1, QgsPoint( p2.x(), p1.y() ) ), false );
            QString height = formatMeasurement( mDa.measureLine( p1, QgsPoint( p1.x(), p2.y() ) ), false );
            measurements.append( QString( "(%1 x %2)" ).arg( width ).arg( height ) );
          }
        }
        break;
      case MEASURE_CIRCLE:
        if ( dynamic_cast<QgsCurvePolygonV2*>( geometry ) )
        {
          QgsCurveV2* ring = static_cast<QgsCurvePolygonV2*>( geometry )->exteriorRing();
          QgsLineStringV2* polyline = ring->curveToLine();
          double area = mDa.measurePolygon( polyline );
          mPartMeasurements.append( area );
          measurements.append( formatMeasurement( area, true ) );
          delete polyline;
          QgsPointV2 p1 = ring->vertexAt( QgsVertexId( 0, 0, 0 ) );
          QgsPointV2 p2 = ring->vertexAt( QgsVertexId( 0, 0, 1 ) );
          measurements.append( QApplication::translate( "QgsGeometryRubberBand", "Radius: %1" ).arg( formatMeasurement( mDa.measureLine( QgsPoint( p1.x(), p1.y() ), QgsPoint( p2.x(), p2.y() ) ), false ) ) );
        }
        break;
      case MEASURE_NONE:
        break;
    }
  }
  if ( !measurements.isEmpty() )
  {
    addMeasurements( measurements, geometry->centroid() );
  }
}

QString QgsGeometryRubberBand::formatMeasurement( double value, bool isArea ) const
{
  int decimals = QSettings().value( "/Qgis/measure/decimalplaces", "2" ).toInt();
  QGis::UnitType measureUnits = mMapCanvas->mapSettings().mapUnits();
  mDa.convertMeasurement( value, measureUnits, mDisplayUnits, isArea );
  return mDa.textUnit( value, decimals, mDisplayUnits, isArea );
}

QString QgsGeometryRubberBand::formatAngle( double value ) const
{
  int decimals = QSettings().value( "/Qgis/measure/decimalplaces", "2" ).toInt();
  switch ( mAngleUnit )
  {
    case ANGLE_DEGREES:
      return QString( "%1 deg" ).arg( QLocale::system().toString( value * 180. / M_PI, 'f', decimals ) );
    case ANGLE_RADIANS:
      return QString( "%1 rad" ).arg( QLocale::system().toString( value, 'f', decimals ) );
    case ANGLE_GRADIANS:
      return QString( "%1 gon" ).arg( QLocale::system().toString( value * 200. / M_PI, 'f', decimals ) );
    case ANGLE_MIL:
      return QString( "%1 mil" ).arg( QLocale::system().toString( value * 3200. / M_PI, 'f', decimals ) );
  }
  return "";
}

void QgsGeometryRubberBand::addMeasurements( const QStringList& measurements, const QgsPointV2& mapPos )
{
  if ( measurements.isEmpty() )
  {
    return;
  }
  QGraphicsTextItem* label = new QGraphicsTextItem();
  mMapCanvas->scene()->addItem( label );
  int red = QSettings().value( "/Qgis/default_measure_color_red", 222 ).toInt();
  int green = QSettings().value( "/Qgis/default_measure_color_green", 155 ).toInt();
  int blue = QSettings().value( "/Qgis/default_measure_color_blue", 67 ).toInt();
  label->setDefaultTextColor( QColor( red, green, blue ) );
  QFont font = label->font();
  font.setBold( true );
  label->setFont( font );
  label->setPos( toCanvasCoordinates( QgsPoint( mapPos.x(), mapPos.y() ) ) );
  QString html = QString( "<div style=\"background: rgba(255, 255, 255, 159); padding: 5px; border-radius: 5px;\">" ) + measurements.join( "</div><div style=\"background: rgba(255, 255, 255, 159); padding: 5px; border-radius: 5px;\">" ) + QString( "</div>" );
  label->setHtml( html );
  label->setZValue( zValue() + 1 );
  mMeasurementLabels.append( label );
}

/*
 * Copyright (c) 2011, Dirk Thomas, TU Darmstadt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the TU Darmstadt nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <rqt_image_view/ratio_layouted_frame.h>

#include <assert.h>
#include <QMouseEvent>

namespace rqt_image_view {

RatioLayoutedFrame::RatioLayoutedFrame(QWidget* parent, Qt::WindowFlags flags)
  : QFrame()
  , outer_layout_(NULL)
  , aspect_ratio_(4, 3)
  , drag_flag_(false)
  , image_freeze_(false)
  , roi_select_enabled_flag_(false)
  , smoothImage_(false)
{
  connect(this, SIGNAL(delayed_update()), this, SLOT(update()), Qt::QueuedConnection);
}

RatioLayoutedFrame::~RatioLayoutedFrame()
{
}

const QImage& RatioLayoutedFrame::getImage() const
{
  return qimage_;
}

QImage RatioLayoutedFrame::getImageCopy() const
{
  QImage img;
  qimage_mutex_.lock();
  img = qimage_.copy();
  qimage_mutex_.unlock();
  return img;
}

void RatioLayoutedFrame::setImage(const QImage& image)//, QMutex* image_mutex)
{
  qimage_mutex_.lock();
  qimage_ = image.copy();
  setAspectRatio(qimage_.width(), qimage_.height());
  qimage_mutex_.unlock();
  emit delayed_update();
}

void RatioLayoutedFrame::resizeToFitAspectRatio()
{
  QRect rect = contentsRect();

  // reduce longer edge to aspect ration
  double width;
  double height;

  if (outer_layout_)
  {
    width = outer_layout_->contentsRect().width();
    height = outer_layout_->contentsRect().height();
  }
  else
  {
    // if outer layout isn't available, this will use the old
    // width and height, but this can shrink the display image if the
    // aspect ratio changes.
    width = rect.width();
    height = rect.height();
  }

  double layout_ar = width / height;
  const double image_ar = double(aspect_ratio_.width()) / double(aspect_ratio_.height());
  if (layout_ar > image_ar)
  {
    // too large width
    width = height * image_ar;
  }
  else
  {
    // too large height
    height = width / image_ar;
  }
  rect.setWidth(int(width + 0.5));
  rect.setHeight(int(height + 0.5));

  // resize taking the border line into account
  int border = lineWidth();
  resize(rect.width() + 2 * border, rect.height() + 2 * border);
}

void RatioLayoutedFrame::setOuterLayout(QHBoxLayout* outer_layout)
{
  outer_layout_ = outer_layout;
}

void RatioLayoutedFrame::setInnerFrameMinimumSize(const QSize& size)
{
  int border = lineWidth();
  QSize new_size = size;
  new_size += QSize(2 * border, 2 * border);
  setMinimumSize(new_size);
  emit delayed_update();
}

void RatioLayoutedFrame::setInnerFrameMaximumSize(const QSize& size)
{
  int border = lineWidth();
  QSize new_size = size;
  new_size += QSize(2 * border, 2 * border);
  setMaximumSize(new_size);
  emit delayed_update();
}

void RatioLayoutedFrame::setInnerFrameFixedSize(const QSize& size)
{
  setInnerFrameMinimumSize(size);
  setInnerFrameMaximumSize(size);
}

void rqt_image_view::RatioLayoutedFrame::roi_select_enabled(bool checked)
{
   roi_select_enabled_flag_ = checked;
}

void RatioLayoutedFrame::mouseReleaseEvent(QMouseEvent *event)
{
    if(drag_flag_ && roi_select_enabled_flag_ && event->button() == Qt::LeftButton)
    {
        drag_flag_ = false;
        image_freeze_ = false;

        //Create a Qrect with proper size for publishing:
        QRect image_window_rect;
        int qimage_width, qimage_height;
        qimage_mutex_.lock();
        if (!qimage_.isNull())
        {
            image_window_rect = contentsRect();
            qimage_width = std::abs(qimage_.width());
            qimage_height = std::abs(qimage_.height());
        }
        else
        {
            qimage_mutex_.unlock();
            return;//Image is null so dont need to do anything
        }
        qimage_mutex_.unlock();
        //Find Final rectangle coords
        int left, top, width, height;
        if(roi_rect_.width() < 0){
            left = roi_rect_.right();
            width = -roi_rect_.width();
        }
        else{
            left = roi_rect_.left();
            width = roi_rect_.width();
        }
        if(roi_rect_.height() < 0){
            top = roi_rect_.bottom();
            height = -roi_rect_.height();
        }
        else{
            top = roi_rect_.top();
            height = roi_rect_.height();
        }

        //Account for -ve left and -ve top coords and width and height being out of bounds:
        if(left < 0)
        {
            width = width + left;
            left = 0;
        }
        else if(width > image_window_rect.width())
        {
            width = image_window_rect.width();
        }

        if(top < 0)
        {
            height = height + top;
            top = 0;
        }
        else if(height > image_window_rect.height())
        {
            height = image_window_rect.height();
        }
        //scale:
        left = (left*qimage_width)/image_window_rect.width();
        top = (top*qimage_height)/image_window_rect.height();
        width = (width*qimage_width)/image_window_rect.width();
        height = (height*qimage_height)/image_window_rect.height();
        emit roi_selected(QRect(left,top,width,height));
    }
}

void RatioLayoutedFrame::mouseMoveEvent(QMouseEvent *event)
{
   //Dragging the mouse:
    if(drag_flag_ && roi_select_enabled_flag_)
    {
        int width = (event->x() - roi_rect_.x());
        int height = (event->y() - roi_rect_.y());
        roi_rect_.setHeight(height);
        roi_rect_.setWidth(width);
        emit delayed_update();//Will force update of image
    }
}

void RatioLayoutedFrame::mousePressEvent(QMouseEvent *event)
{
    if(roi_select_enabled_flag_)
    {
      if(event->buttons() == Qt::LeftButton)
      {
        roi_rect_.setX(event->x());
        roi_rect_.setY(event->y());
        if(image_freeze_ == true)
          drag_flag_ = true;
      }
      else if(event->buttons() == Qt::RightButton)
      {
        image_freeze_ = true;
        emit roi_started();
      }
    }
    if(event->button() == Qt::LeftButton)
    {
      emit mouseLeft(event->x(), event->y());
    }
    QFrame::mousePressEvent(event);
}

void RatioLayoutedFrame::setAspectRatio(unsigned short width, unsigned short height)
{
  int divisor = greatestCommonDivisor(width, height);
  if (divisor != 0) {
    aspect_ratio_.setWidth(width / divisor);
    aspect_ratio_.setHeight(height / divisor);
  }
}

void RatioLayoutedFrame::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);
  qimage_mutex_.lock();
  if (!qimage_.isNull())
  {
    resizeToFitAspectRatio();
    // TODO: check if full draw is really necessary
    //QPaintEvent* paint_event = dynamic_cast<QPaintEvent*>(event);
    //painter.drawImage(paint_event->rect(), qimage_);
    if (!smoothImage_) {
      painter.drawImage(contentsRect(), qimage_);
    } else {
      if (contentsRect().width() == qimage_.width()) {
        painter.drawImage(contentsRect(), qimage_);
      } else {
        QImage image = qimage_.scaled(contentsRect().width(), contentsRect().height(),
                                      Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawImage(contentsRect(), image);
      }
    }
    if(drag_flag_)//If we are dragging, we plot the rectangle over the image
    {
      painter.setPen(Qt::red);
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(roi_rect_);
      painter.end();
    }
  } else {
    // default image with gradient
    QLinearGradient gradient(0, 0, frameRect().width(), frameRect().height());
    gradient.setColorAt(0, Qt::white);
    gradient.setColorAt(1, Qt::black);
    painter.setBrush(gradient);
    painter.drawRect(0, 0, frameRect().width() + 1, frameRect().height() + 1);
  }
  qimage_mutex_.unlock();
}

int RatioLayoutedFrame::greatestCommonDivisor(int a, int b)
{
  if (b==0)
  {
    return a;
  }
  return greatestCommonDivisor(b, a % b);
}

void RatioLayoutedFrame::onSmoothImageChanged(bool checked) {
  smoothImage_ = checked;
}

}

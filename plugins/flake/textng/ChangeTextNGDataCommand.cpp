/* This file is part of the KDE project

   Copyright 2017 Boudewijn Rempt <boud@valdyas.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#include "ChangeTextNGDataCommand.h"

#include <math.h>
#include <klocalizedstring.h>
#include <KoImageData.h>

#include "TextNGShape.h"

ChangeTextNGDataCommand::ChangeTextNGDataCommand(TextNGShape *shape,
                                                   KUndo2Command *parent)
    : KUndo2Command(parent)
    , m_shape(shape)
{
    Q_ASSERT(shape);
    setText(kundo2_i18n("Change TextNG"));
}

ChangeTextNGDataCommand::~ChangeTextNGDataCommand()
{
}

void ChangeTextNGDataCommand::redo()
{
    m_shape->update();
    //m_shape->setCompressedContents(m_newImageData, m_newTextNGType);
    m_shape->update();
}

void ChangeTextNGDataCommand::undo()
{
    m_shape->update();
    //m_shape->setCompressedContents(m_oldImageData, m_oldTextNGType);
    m_shape->update();
}
/*
 *  Copyright (c) 2015 Michael Abrahams <miabraha@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <QString>
#include <QHash>
#include <QGlobalStatic>
#include <QFile>
#include <QDomElement>
#include <KSharedConfig>
#include <klocalizedstring.h>
#include <KisShortcutsDialog.h>
#include <KConfigGroup>

#include "kis_debug.h"
#include "KoResourcePaths.h"
#include "kis_icon_utils.h"
#include "kactioncollection.h"
#include "kactioncategory.h"


#include "kis_action_registry.h"


namespace {

    /**
     * We associate several pieces of information with each shortcut. The first
     * piece of information is a QDomElement, containing the raw data from the
     * .action XML file. The second and third are QKeySequences, the first of
     * which is the default shortcut, the last of which is any custom shortcut.
     * The last two are the KActionCollection and KActionCategory used to
     * organize the shortcut editor.
     */
    struct ActionInfoItem {
        QDomElement  xmlData;
        QKeySequence defaultShortcut;
        QKeySequence customShortcut;
        QString      collectionName;
        QString      categoryName;
    };

    QKeySequence getShortcutFromXml(QDomElement node) {
        return node.firstChildElement("shortcut").text();
    };
    ActionInfoItem emptyActionInfo;  // Used as default return value


    QString quietlyTranslate(const QString &s) {
        if (s.isEmpty()) {
            return s;
        }
        if (i18n(s.toUtf8().constData()).isEmpty()) {
            dbgAction << "No translation found for" << s;
            return s;
        }
        return i18n(s.toUtf8().constData());
    };


    QKeySequence preferredShortcut(ActionInfoItem action) {
        if (action.customShortcut.isEmpty()) {
            return action.defaultShortcut;
        } else {
            return action.customShortcut;
        }
    };

};



class Q_DECL_HIDDEN KisActionRegistry::Private
{
public:

    Private(KisActionRegistry *_q) : q(_q) {};

    // This is the main place containing ActionInfoItems.
    QMap<QString, ActionInfoItem> actionInfoList;
    void loadActionFiles();
    void loadActionCollections();
    void loadCustomShortcuts(QString filename = QStringLiteral("kritashortcutsrc"));
    ActionInfoItem actionInfo(const QString &name) {
        return actionInfoList.value(name, emptyActionInfo);
    };

    KisActionRegistry *q;
    KActionCollection * defaultActionCollection;
    QMap<QString, KActionCollection*> actionCollections;
};


Q_GLOBAL_STATIC(KisActionRegistry, s_instance);

KisActionRegistry *KisActionRegistry::instance()
{
    return s_instance;
};


KisActionRegistry::KisActionRegistry()
    : d(new KisActionRegistry::Private(this))
{
    d->loadActionFiles();
    d->loadCustomShortcuts();
}

// Not the most efficient logic, but simple and readable.
QKeySequence KisActionRegistry::getPreferredShortcut(const QString &name)
{
    return preferredShortcut(d->actionInfo(name));
};

QKeySequence KisActionRegistry::getCategory(const QString &name)
{
    return d->actionInfo(name).categoryName;
};

QStringList KisActionRegistry::allActions()
{
    return d->actionInfoList.keys();
};

KActionCollection * KisActionRegistry::getDefaultCollection()
{
    return d->actionCollections.value("Krita");
};

void KisActionRegistry::addAction(const QString &name, QAction *a)
{
    auto info = d->actionInfo(name);

    KActionCollection *collection = d->actionCollections.value(info.collectionName);
    if (!collection) {
        qDebug() << "No collection found for action" << name;
        return;
    }
    if (collection->action(name)) {
        dbgAction << "duplicate action" << name << "in collection" << collection->componentName();
    }
    else {
    }
    collection->addCategorizedAction(name, a, info.categoryName);
};


void KisActionRegistry::notifySettingsUpdated()
{
    d->loadCustomShortcuts();
};

QAction * KisActionRegistry::makeQAction(const QString &name, QObject *parent)
{

    QAction * a = new QAction(parent);
    if (!d->actionInfoList.contains(name)) {
        dbgAction << "Warning: requested data for unknown action" << name;
        return a;
    }
    propertizeAction(name, a);
    return a;
};


void KisActionRegistry::configureShortcuts(KActionCollection *ac)
{
    KisShortcutsDialog dlg;

    for (auto i = d->actionCollections.constBegin(); i != d->actionCollections.constEnd(); i++ ) {
        dlg.addCollection(i.value(), i.key());
    }

    /* Testing */
    // QStringList mainWindowActions;
    // foreach (auto a, ac->actions()) {
    //     mainWindowActions << a->objectName();
    // }
    // dlg.addCollection(ac, "TESTING: XMLGUI-MAINWINDOW");

   dlg.configure();  // Show the dialog.

   d->loadCustomShortcuts();
}


void KisActionRegistry::updateShortcut(const QString &name, QAction *action)
{
    action->setShortcut(preferredShortcut(d->actionInfo(name)));
}


bool KisActionRegistry::propertizeAction(const QString &name, QAction * a)
{

    ActionInfoItem info = d->actionInfo(name);
    QDomElement actionXml = info.xmlData;
    if (actionXml.text().isEmpty()) {
        dbgAction << "No XML data found for action" << name;
        return false;
    }


    // Convenience macros to extract text of a child node.
    auto getChildContent      = [=](QString node){return actionXml.firstChildElement(node).text();};
    // i18n requires converting format from QString.
    auto getChildContent_i18n = [=](QString node){return quietlyTranslate(getChildContent(node));};

    // Note: the fields in the .action documents marked for translation are determined by extractrc.
    QString icon                 = getChildContent("icon");
    QString text                 = getChildContent("text");
    QString whatsthis            = getChildContent_i18n("whatsThis");
    QString toolTip              = getChildContent_i18n("toolTip");
    QString statusTip            = getChildContent_i18n("statusTip");
    QString iconText             = getChildContent_i18n("iconText");
    bool isCheckable             = getChildContent("isCheckable") == QString("true");




    a->setObjectName(name); // This is helpful, should be added more places in Krita
    a->setIcon(KisIconUtils::loadIcon(icon.toLatin1()));
    a->setText(text);
    a->setObjectName(name);
    a->setWhatsThis(whatsthis);
    a->setToolTip(toolTip);
    a->setStatusTip(statusTip);
    a->setIconText(iconText);
    a->setCheckable(isCheckable);


    a->setShortcut(preferredShortcut(info));
    auto propertizedShortcut = qVariantFromValue(QList<QKeySequence>() << info.defaultShortcut);
    a->setProperty("defaultShortcuts", propertizedShortcut);



    // TODO: check for colliding shortcuts, either with some code like this, or
    // by relying on the code existing inside kactioncollection
    //
    // QMap<QKeySequence, QAction*> existingShortcuts;
    // foreach(QAction* action, actionCollection->actions()) {
    //     if(action->shortcut() == QKeySequence(0)) {
    //         continue;
    //     }
    //     if (existingShortcuts.contains(action->shortcut())) {
    //         dbgAction << QString("Actions %1 and %2 have the same shortcut: %3") \
    //             .arg(action->text())                                             \
    //             .arg(existingShortcuts[action->shortcut()]->text())              \
    //             .arg(action->shortcut());
    //     }
    //     else {
    //         existingShortcuts[action->shortcut()] = action;
    //     }
    // }


    return true;
}





void KisActionRegistry::Private::loadActionFiles()
{

    KoResourcePaths::addResourceType("kis_actions", "data", "krita/actions");
    auto searchType = KoResourcePaths::Recursive | KoResourcePaths::NoDuplicates;
    QStringList actionDefinitions =
        KoResourcePaths::findAllResources("kis_actions", "*.action", searchType);

    // Extract actions all XML .action files.
    foreach(const QString &actionDefinition, actionDefinitions)  {
        QDomDocument doc;
        QFile f(actionDefinition);
        f.open(QFile::ReadOnly);
        doc.setContent(f.readAll());

        QDomElement base       = doc.documentElement(); // "ActionCollection" outer group
        QString collectionName = base.attribute("name");
        QString version        = base.attribute("version");
        if (version != "2") {
            errAction << ".action XML file" << actionDefinition << "has incorrect version; skipping.";
            continue;
        }


        KActionCollection *actionCollection;
        if (!actionCollections.contains(collectionName)) {
            actionCollection = new KActionCollection(q, collectionName);
            actionCollections.insert(collectionName, actionCollection);
            dbgAction << "Adding a new action collection " << collectionName;
        } else {
            actionCollection = actionCollections.value(collectionName);
        }

        // Loop over <Actions> nodes. Each of these corresponds to a
        // KActionCategory, producing a group of actions in the shortcut dialog.
        QDomElement actions = base.firstChild().toElement();
        while (!actions.isNull()) {

            // <text> field
            QDomElement categoryTextNode = actions.firstChild().toElement();
            QString categoryName         = quietlyTranslate(categoryTextNode.text());
            KActionCategory *category    = actionCollection->getCategory(categoryName);
            dbgAction << "Using category" << categoryName;

            // <action></action> tags
            QDomElement actionXml  = categoryTextNode.nextSiblingElement();

            // Loop over individual actions
            while (!actionXml.isNull()) {
                if (actionXml.tagName() == "Action") {
                    // Read name from format <Action name="save">
                    QString name      = actionXml.attribute("name");

                    // Bad things
                    if (name.isEmpty()) {
                        errAction << "Unnamed action in definitions file " << actionDefinition;
                    }

                    else if (actionInfoList.contains(name)) {
                        // errAction << "NOT COOL: Duplicated action name from xml data: " << name;
                    }

                    else {
                        ActionInfoItem info;
                        info.xmlData         = actionXml;
                        info.defaultShortcut = getShortcutFromXml(actionXml);
                        info.customShortcut  = QKeySequence();
                        info.categoryName    = categoryName;
                        info.collectionName  = collectionName;

                        // dbgAction << "default shortcut for" << name << " - " << info.defaultShortcut;
                        actionInfoList.insert(name,info);
                    }

                }
                actionXml = actionXml.nextSiblingElement();
            }
            actions = actions.nextSiblingElement();
        }

    }

};

void KisActionRegistry::Private::loadCustomShortcuts(QString filename)
{
    Q_UNUSED(filename);

    const KConfigGroup localShortcuts(KSharedConfig::openConfig("kritashortcutsrc"),
                                      QStringLiteral("Shortcuts"));


    if (!localShortcuts.exists()) {
        return;
    }

    for (auto i = actionInfoList.begin(); i != actionInfoList.end(); ++i) {
        if (localShortcuts.hasKey(i.key())) {
            QString entry = localShortcuts.readEntry(i.key(), QString());
            i.value().customShortcut = QKeySequence(entry);
        }
    }
};
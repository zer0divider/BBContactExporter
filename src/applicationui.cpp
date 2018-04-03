/*
 * Copyright (c) 2011-2015 BlackBerry Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "applicationui.hpp"

#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/AbstractPane>
#include <bb/cascades/LocaleHandler>
#include <bb/system/SystemToast>
#include <bb/system/SystemUiPosition>
#include <bb/pim/contacts/ContactService>
#include <bb/system/SystemProgressDialog>

using namespace bb::cascades;
using namespace bb::system;
using namespace bb::pim::contacts;

ApplicationUI::ApplicationUI() :
        QObject()
{
    // prepare the localization
    m_pTranslator = new QTranslator(this);
    m_pLocaleHandler = new LocaleHandler(this);

    bool res = QObject::connect(m_pLocaleHandler, SIGNAL(systemLanguageChanged()), this, SLOT(onSystemLanguageChanged()));
    // This is only available in Debug builds
    Q_ASSERT(res);
    // Since the variable is not used in the app, this is added to avoid a
    // compiler warning
    Q_UNUSED(res);

    // initial load
    onSystemLanguageChanged();

    // Create scene document from main.qml asset, the parent is set
    // to ensure the document gets destroyed properly at shut down.
    QmlDocument *qml = QmlDocument::create("asset:///main.qml").parent(this);
    qml->setContextProperty("_app", this);

    // Create root object for the UI
    AbstractPane *root = qml->createRootObject<AbstractPane>();


    // Set created root object as the application scene
    Application::instance()->setScene(root);
}

void ApplicationUI::onSystemLanguageChanged()
{
    QCoreApplication::instance()->removeTranslator(m_pTranslator);
    // Initiate, load and install the application translation files.
    QString locale_string = QLocale().name();
    QString file_name = QString("CascadesProject_%1").arg(locale_string);
    if (m_pTranslator->load(file_name, "app/native/qm")) {
        QCoreApplication::instance()->installTranslator(m_pTranslator);
    }
}

void ApplicationUI::exportContacts()
{
    //starting contact service
    ContactService cs;
    ContactListFilters filter;
    QList<Contact> allContacts = cs.contacts(filter);

    //creating new file and writing out
     QDir dir;
     dir.mkpath("data");
     QFile vcardfile("data/contacts.vcf");
     if(!vcardfile.open(QIODevice::WriteOnly))
     {
         SystemToast *toast = new SystemToast(this);
         toast->setBody("Error: could not write to file");
         toast->setPosition(SystemUiPosition::MiddleCenter);
         toast->show();
         return;
     }
     //creating dialog with progress bar
     SystemProgressDialog* dialog;
     dialog = new SystemProgressDialog("Cancel");
     dialog->setTitle("Exporting...");
     dialog->setBody("");
     dialog->setState(SystemUiProgressState::Active);
     dialog->setProgress(0);
     dialog->show();
     //fetching all contact details and write them into file
    int last_percentage = 0;
    int percentage = 0;
    for(int i =0; i < allContacts.size(); i++)
    {
        QByteArray data = cs.contactToVCard(cs.contactDetails(allContacts[i].id()));
        vcardfile.write(data);
        //updating state
        percentage = (100*(i+1))/static_cast<float>(allContacts.size());
        if(percentage != last_percentage)
        {
            dialog->setProgress(percentage);
            dialog->update();
            last_percentage = percentage;
        }
    }
    vcardfile.close();
    dialog->setProgress(100);
    dialog->update();

    dialog->deleteLater();
    //make invocation to email program
    InvokeManager invokeManager;
    InvokeRequest request;
    request.setTarget("sys.pim.uib.email.hybridcomposer");
    request.setAction("bb.action.SHARE");
    request.setMimeType("text/vcard");
    QString URI = "file://"+QDir::currentPath()+"/data/contacts.vcf";
    request.setUri(URI);
    _invokeTargetReply = invokeManager.invoke(request);
    _invokeTargetReply->setParent(this);
    QObject::connect(_invokeTargetReply, SIGNAL(finished()), this, SLOT(onInvokeResult()));
}

void ApplicationUI::onInvokeResult()
{
    // Check for errors
    QString out;
      switch(_invokeTargetReply->error()) {
          // Invocation could not find the target;
          // did we use the right target ID?
          case InvokeReplyError::NoTarget: {
              out = "Error: no target!";
              break;
          }

          // There was a problem with the invocation request;
          // did we set all of the values correctly?
          case InvokeReplyError::BadRequest: {
              out = "Error: bad request!";
              break;
          }

          // Something went completely
          // wrong inside the invocation request,
          // so find an alternate route
          case InvokeReplyError::Internal: {
              out = "Error: internal!";
              break;
          }

          // Message received if the invocation request is successful
          default:
              break;
      }

      // Free the resources that were allocated for the reply
      delete _invokeTargetReply;
      if(!out.isEmpty())
      {
          SystemToast *toast = new SystemToast(this);
          toast->setBody(out);
          toast->setPosition(SystemUiPosition::MiddleCenter);
          toast->show();
      }
}

/* $Id: UIGlobalSettingsNetwork.cpp 66678 2017-04-26 15:16:31Z vboxsync $ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsNetwork class implementation.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QHeaderView>

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIIconPool.h"
# include "UIConverter.h"
# include "UIMessageCenter.h"
# include "UIGlobalSettingsNetwork.h"
# include "UIGlobalSettingsNetworkDetailsNAT.h"
# include "UIGlobalSettingsNetworkDetailsHost.h"

/* COM includes: */
# include "CDHCPServer.h"
# include "CNATNetwork.h"
# include "CHostNetworkInterface.h"

/* Other VBox includes: */
# include <iprt/cidr.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** Global settings: Network page data structure. */
struct UIDataSettingsGlobalNetwork
{
    /** Constructs data. */
    UIDataSettingsGlobalNetwork() {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalNetwork & /* other */) const { return true; }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalNetwork & /* other */) const { return false; }
};


/** Global settings: Network page: NAT network tree-widget item. */
class UIItemNetworkNAT : public QITreeWidgetItem, public UIDataSettingsGlobalNetworkNAT
{
public:

    /** Constructs item. */
    UIItemNetworkNAT();

    /** Updates item fields from data. */
    void updateFields();
    /** Updates item data from fields. */
    void updateData();

    /** Performs validation, updates @a messages list if something is wrong. */
    bool validate(UIValidationMessage &messages);

    /** Returns item name. */
    QString name() const { return m_strName; }
    /** Returns new item name. */
    QString newName() const { return m_strNewName; }

    /** Returns IPv4 port forwarding rules. */
    const UIPortForwardingDataList &ipv4rules() const { return m_ipv4rules; }
    /** Defines IPv4 port forwarding rules. */
    void setIpv4rules(const UIPortForwardingDataList &ipv4rules) { m_ipv4rules = ipv4rules; }
    /** Returns IPv6 port forwarding rules. */
    const UIPortForwardingDataList &ipv6rules() const { return m_ipv6rules; }
    /** Defines IPv6 port forwarding rules. */
    void setIpv6rules(const UIPortForwardingDataList &ipv6rules) { m_ipv6rules = ipv6rules; }

protected:

    /** Returns default text. */
    virtual QString defaultText() const /* override */;

private:

    /** Holds IPv4 port forwarding rules. */
    UIPortForwardingDataList m_ipv4rules;
    /** Holds IPv6 port forwarding rules. */
    UIPortForwardingDataList m_ipv6rules;
};


/** Global settings: Network page: Host network tree-widget item. */
class UIItemNetworkHost : public QITreeWidgetItem, public UIDataSettingsGlobalNetworkHost
{
public:

    /** Constructs item. */
    UIItemNetworkHost();

    /** Updates item fields from data. */
    void updateFields();

    /** Performs validation, updates @a messages list if something is wrong. */
    bool validate(UIValidationMessage &messages);

    /** Returns item name. */
    QString name() const { return m_interface.m_strName; }
};


/*********************************************************************************************************************************
*   Class UIItemNetworkNAT implementation.                                                                                       *
*********************************************************************************************************************************/

UIItemNetworkNAT::UIItemNetworkNAT()
    : QITreeWidgetItem()
{
}

void UIItemNetworkNAT::updateFields()
{
    /* Compose item name/tool-tip: */
    const QString strHeader("<tr><td><nobr>%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    const QString strSubHeader("<tr><td><nobr>&nbsp;&nbsp;%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    QString strToolTip;

    /* Item name was not changed: */
    setCheckState(0, m_fEnabled ? Qt::Checked : Qt::Unchecked);
    if (m_strNewName == m_strName)
    {
        /* Just use the old one: */
        setText(1, m_strName);
        strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("Network Name"), m_strName);
    }
    /* If name was changed: */
    else
    {
        /* We should explain that: */
        const QString oldName = m_strName;
        const QString newName = m_strNewName.isEmpty() ? UIGlobalSettingsNetwork::tr("[empty]") : m_strNewName;
        setText(1, UIGlobalSettingsNetwork::tr("%1 (renamed from %2)").arg(newName, oldName));
        strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("Old Network Name"), m_strName);
        strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("New Network Name"), m_strNewName);
    }

    /* Other tool-tip information: */
    strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("Network CIDR"), m_strCIDR);
    strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("Supports DHCP"),
                                m_fSupportsDHCP ? UIGlobalSettingsNetwork::tr("yes") : UIGlobalSettingsNetwork::tr("no"));
    strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("Supports IPv6"),
                                m_fSupportsIPv6 ? UIGlobalSettingsNetwork::tr("yes") : UIGlobalSettingsNetwork::tr("no"));
    if (m_fSupportsIPv6 && m_fAdvertiseDefaultIPv6Route)
        strToolTip += strSubHeader.arg(UIGlobalSettingsNetwork::tr("Default IPv6 route"), UIGlobalSettingsNetwork::tr("yes"));

    /* Assign tool-tip finally: */
    setToolTip(1, strToolTip);
}

void UIItemNetworkNAT::updateData()
{
    /* Update data: */
    m_fEnabled = checkState(0) == Qt::Checked;
}

bool UIItemNetworkNAT::validate(UIValidationMessage &message)
{
    /* Pass by default: */
    bool fPass = true;

    /* NAT network name validation: */
    bool fNameValid = true;
    if (m_strNewName.isEmpty())
    {
        /* Emptiness validation: */
        message.second << UIGlobalSettingsNetwork::tr("No new name specified for the NAT network previously called <b>%1</b>.").arg(m_strName);
        fNameValid = false;
        fPass = false;
    }

    /* NAT network CIDR validation: */
    if (m_strCIDR.isEmpty())
    {
        /* Emptiness validation: */
        if (fNameValid)
            message.second << UIGlobalSettingsNetwork::tr("No CIDR specified for the NAT network <b>%1</b>.").arg(m_strNewName);
        else
            message.second << UIGlobalSettingsNetwork::tr("No CIDR specified for the NAT network previously called <b>%1</b>.").arg(m_strName);
        fPass = false;
    }
    else
    {
        /* Correctness validation: */
        RTNETADDRIPV4 network, mask;
        int rc = RTCidrStrToIPv4(m_strCIDR.toUtf8().constData(), &network, &mask);
        if (RT_FAILURE(rc))
        {
            if (fNameValid)
                message.second << UIGlobalSettingsNetwork::tr("Invalid CIDR specified (<i>%1</i>) for the NAT network <b>%2</b>.")
                                                              .arg(m_strCIDR, m_strNewName);
            else
                message.second << UIGlobalSettingsNetwork::tr("Invalid CIDR specified (<i>%1</i>) for the NAT network previously called <b>%2</b>.")
                                                              .arg(m_strCIDR, m_strName);
            fPass = false;
        }
    }

    /* Return result: */
    return fPass;
}

QString UIItemNetworkNAT::defaultText() const
{
    return m_fEnabled ?
           tr("%1, %2", "col.2 text, col.1 name")
             .arg(text(1))
             .arg(parentTree()->headerItem()->text(0)) :
           tr("%1",     "col.2 text")
             .arg(text(1));
}


/*********************************************************************************************************************************
*   Class UIItemNetworkHost implementation.                                                                                      *
*********************************************************************************************************************************/

UIItemNetworkHost::UIItemNetworkHost()
    : QITreeWidgetItem()
{
}

void UIItemNetworkHost::updateFields()
{
    /* Compose item name/tool-tip: */
    setText(0, m_interface.m_strName);
    const QString strHeader("<tr><td><nobr>%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    const QString strSubHeader("<tr><td><nobr>&nbsp;&nbsp;%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    QString strToolTip;

    /* Host interface information: */
    strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("Adapter"))
                           .arg(m_interface.m_fDhcpClientEnabled ?
                                UIGlobalSettingsNetwork::tr("Automatically configured", "interface") :
                                UIGlobalSettingsNetwork::tr("Manually configured", "interface"));
    if (!m_interface.m_fDhcpClientEnabled)
    {
        strToolTip += strSubHeader.arg(UIGlobalSettingsNetwork::tr("IPv4 Address"))
                                  .arg(m_interface.m_strInterfaceAddress.isEmpty() ?
                                       UIGlobalSettingsNetwork::tr ("Not set", "address") :
                                       m_interface.m_strInterfaceAddress) +
                      strSubHeader.arg(UIGlobalSettingsNetwork::tr("IPv4 Network Mask"))
                                  .arg(m_interface.m_strInterfaceMask.isEmpty() ?
                                       UIGlobalSettingsNetwork::tr ("Not set", "mask") :
                                       m_interface.m_strInterfaceMask);
        if (m_interface.m_fIpv6Supported)
        {
            strToolTip += strSubHeader.arg(UIGlobalSettingsNetwork::tr("IPv6 Address"))
                                      .arg(m_interface.m_strInterfaceAddress6.isEmpty() ?
                                           UIGlobalSettingsNetwork::tr("Not set", "address") :
                                           m_interface.m_strInterfaceAddress6) +
                          strSubHeader.arg(UIGlobalSettingsNetwork::tr("IPv6 Network Mask Length"))
                                      .arg(m_interface.m_strInterfaceMaskLength6.isEmpty() ?
                                           UIGlobalSettingsNetwork::tr("Not set", "length") :
                                           m_interface.m_strInterfaceMaskLength6);
        }
    }

    /* DHCP server information: */
    strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("DHCP Server"))
                           .arg(m_dhcpserver.m_fDhcpServerEnabled ?
                                UIGlobalSettingsNetwork::tr("Enabled", "server") :
                                UIGlobalSettingsNetwork::tr("Disabled", "server"));
    if (m_dhcpserver.m_fDhcpServerEnabled)
    {
        strToolTip += strSubHeader.arg(UIGlobalSettingsNetwork::tr("Address"))
                                  .arg(m_dhcpserver.m_strDhcpServerAddress.isEmpty() ?
                                       UIGlobalSettingsNetwork::tr("Not set", "address") :
                                       m_dhcpserver.m_strDhcpServerAddress) +
                      strSubHeader.arg(UIGlobalSettingsNetwork::tr("Network Mask"))
                                  .arg(m_dhcpserver.m_strDhcpServerMask.isEmpty() ?
                                       UIGlobalSettingsNetwork::tr("Not set", "mask") :
                                       m_dhcpserver.m_strDhcpServerMask) +
                      strSubHeader.arg(UIGlobalSettingsNetwork::tr("Lower Bound"))
                                  .arg(m_dhcpserver.m_strDhcpLowerAddress.isEmpty() ?
                                       UIGlobalSettingsNetwork::tr("Not set", "bound") :
                                       m_dhcpserver.m_strDhcpLowerAddress) +
                      strSubHeader.arg(UIGlobalSettingsNetwork::tr("Upper Bound"))
                                  .arg(m_dhcpserver.m_strDhcpUpperAddress.isEmpty() ?
                                       UIGlobalSettingsNetwork::tr("Not set", "bound") :
                                       m_dhcpserver.m_strDhcpUpperAddress);
    }

    /* Assign tool-tip finally: */
    setToolTip(0, strToolTip);
}

bool UIItemNetworkHost::validate(UIValidationMessage &message)
{
    /* Pass by default: */
    bool fPass = true;

    /* Host interface validation: */
    if (!m_interface.m_fDhcpClientEnabled)
    {
        if (!m_interface.m_strInterfaceAddress.trimmed().isEmpty() &&
            (   !RTNetIsIPv4AddrStr(m_interface.m_strInterfaceAddress.toUtf8().constData())
             || RTNetStrIsIPv4AddrAny(m_interface.m_strInterfaceAddress.toUtf8().constData())))
        {
            message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid IPv4 address.").arg(text(0));
            fPass = false;
        }
        if (!m_interface.m_strInterfaceMask.trimmed().isEmpty() &&
            (   !RTNetIsIPv4AddrStr(m_interface.m_strInterfaceMask.toUtf8().constData())
             || RTNetStrIsIPv4AddrAny(m_interface.m_strInterfaceMask.toUtf8().constData())))
        {
            message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid IPv4 network mask.").arg(text(0));
            fPass = false;
        }
        if (m_interface.m_fIpv6Supported)
        {
            if (!m_interface.m_strInterfaceAddress6.trimmed().isEmpty() &&
                (   !RTNetIsIPv6AddrStr(m_interface.m_strInterfaceAddress6.toUtf8().constData())
                 || RTNetStrIsIPv6AddrAny(m_interface.m_strInterfaceAddress6.toUtf8().constData())))
            {
                message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid IPv6 address.").arg(text(0));
                fPass = false;
            }
            bool fIsMaskPrefixLengthNumber = false;
            const int iMaskPrefixLength = m_interface.m_strInterfaceMaskLength6.trimmed().toInt(&fIsMaskPrefixLengthNumber);
            if (   !fIsMaskPrefixLengthNumber
                || iMaskPrefixLength < 0
                || iMaskPrefixLength > 128)
            {
                message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid IPv6 network mask prefix length.").arg(text(0));
                fPass = false;
            }
        }
    }

    /* DHCP server validation: */
    if (m_dhcpserver.m_fDhcpServerEnabled)
    {
        if (   !RTNetIsIPv4AddrStr(m_dhcpserver.m_strDhcpServerAddress.toUtf8().constData())
            || RTNetStrIsIPv4AddrAny(m_dhcpserver.m_strDhcpServerAddress.toUtf8().constData()))
        {
            message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid DHCP server address.").arg(text(0));
            fPass = false;
        }
        if (   !RTNetIsIPv4AddrStr(m_dhcpserver.m_strDhcpServerMask.toUtf8().constData())
            || RTNetStrIsIPv4AddrAny(m_dhcpserver.m_strDhcpServerMask.toUtf8().constData()))
        {
            message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid DHCP server mask.").arg(text(0));
            fPass = false;
        }
        if (   !RTNetIsIPv4AddrStr(m_dhcpserver.m_strDhcpLowerAddress.toUtf8().constData())
            || RTNetStrIsIPv4AddrAny(m_dhcpserver.m_strDhcpLowerAddress.toUtf8().constData()))
        {
            message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid DHCP server lower address bound.").arg(text(0));
            fPass = false;
        }
        if (   !RTNetIsIPv4AddrStr(m_dhcpserver.m_strDhcpUpperAddress.toUtf8().constData())
            || RTNetStrIsIPv4AddrAny(m_dhcpserver.m_strDhcpUpperAddress.toUtf8().constData()))
        {
            message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid DHCP server upper address bound.").arg(text(0));
            fPass = false;
        }
    }

    /* Return result: */
    return fPass;
}


/*********************************************************************************************************************************
*   Class UIGlobalSettingsNetwork implementation.                                                                                *
*********************************************************************************************************************************/

UIGlobalSettingsNetwork::UIGlobalSettingsNetwork()
    : m_pActionAddNetworkNAT(0), m_pActionEditNetworkNAT(0), m_pActionRemoveNetworkNAT(0)
    , m_pActionAddNetworkHost(0), m_pActionEditNetworkHost(0), m_pActionRemoveNetworkHost(0)
    , m_pCache(0)
{
    /* Prepare: */
    prepare();
}

UIGlobalSettingsNetwork::~UIGlobalSettingsNetwork()
{
    /* Cleanup: */
    cleanup();
}

void UIGlobalSettingsNetwork::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare old network data: */
    UIDataSettingsGlobalNetwork oldNetworkData;

    /* Gather old network data: */
    foreach (const CNATNetwork &network, vboxGlobal().virtualBox().GetNATNetworks())
        loadToCacheFromNetworkNAT(network, m_pCache->child1(network.GetNetworkName()));
    foreach (const CHostNetworkInterface &iface, vboxGlobal().host().GetNetworkInterfaces())
        if (iface.GetInterfaceType() == KHostNetworkInterfaceType_HostOnly)
            loadToCacheFromNetworkHost(iface, m_pCache->child2(iface.GetName()));

    /* Cache old network data: */
    m_pCache->cacheInitialData(oldNetworkData);

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsNetwork::getFromCache()
{
    /* Load old network data from the cache: */
    for (int i = 0; i < m_pCache->childCount1(); ++i)
        createTreeWidgetItemForNetworkNAT(m_pCache->child1(i));
    m_pTreeNetworkNAT->sortByColumn(1, Qt::AscendingOrder);
    m_pTreeNetworkNAT->setCurrentItem(m_pTreeNetworkNAT->topLevelItem(0));
    sltHandleCurrentItemChangeNetworkNAT();
    for (int i = 0; i < m_pCache->childCount2(); ++i)
        createTreeWidgetItemForNetworkHost(m_pCache->child2(i));
    m_pTreeNetworkHost->sortByColumn(0, Qt::AscendingOrder);
    m_pTreeNetworkHost->setCurrentItem(m_pTreeNetworkHost->topLevelItem(0));
    sltHandleCurrentItemChangeNetworkHost();

    /* Revalidate: */
    revalidate();
}

void UIGlobalSettingsNetwork::putToCache()
{
    /* Prepare new network data: */
    UIDataSettingsGlobalNetwork newNetworkData;

    /* Gather new network data: */
    for (int i = 0; i < m_pTreeNetworkNAT->topLevelItemCount(); ++i)
    {
        const UIItemNetworkNAT *pItem = static_cast<UIItemNetworkNAT*>(m_pTreeNetworkNAT->topLevelItem(i));
        m_pCache->child1(pItem->m_strName).cacheCurrentData(*pItem);
        foreach (const UIDataPortForwardingRule &rule, pItem->ipv4rules())
            m_pCache->child1(pItem->m_strName).child1(rule.name).cacheCurrentData(rule);
        foreach (const UIDataPortForwardingRule &rule, pItem->ipv6rules())
            m_pCache->child1(pItem->m_strName).child2(rule.name).cacheCurrentData(rule);
    }
    for (int i = 0; i < m_pTreeNetworkHost->topLevelItemCount(); ++i)
    {
        const UIItemNetworkHost *pItem = static_cast<UIItemNetworkHost*>(m_pTreeNetworkHost->topLevelItem(i));
        m_pCache->child2(pItem->m_interface.m_strName).cacheCurrentData(*pItem);
    }

    /* Cache new network data: */
    m_pCache->cacheCurrentData(newNetworkData);
}

void UIGlobalSettingsNetwork::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Update network data and failing state: */
    setFailed(!saveNetworkData());

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

bool UIGlobalSettingsNetwork::validate(QList<UIValidationMessage> &messages)
{
    /* Pass by default: */
    bool fPass = true;

    /* Validate NAT network items: */
    {
        /* Prepare message: */
        UIValidationMessage message;
        message.first = VBoxGlobal::removeAccelMark(m_pTabWidget->tabText(0));

        /* Validate items first: */
        for (int i = 0; i < m_pTreeNetworkNAT->topLevelItemCount(); ++i)
        {
            UIItemNetworkNAT *pItem = static_cast<UIItemNetworkNAT*>(m_pTreeNetworkNAT->topLevelItem(i));
            if (!pItem->validate(message))
                fPass = false;
        }

        /* And make sure item names are unique: */
        QList<QString> names;
        for (int iItemIndex = 0; iItemIndex < m_pTreeNetworkNAT->topLevelItemCount(); ++iItemIndex)
        {
            UIItemNetworkNAT *pItem = static_cast<UIItemNetworkNAT*>(m_pTreeNetworkNAT->topLevelItem(iItemIndex));
            const QString strItemName(pItem->newName());
            if (strItemName.isEmpty())
                continue;
            if (!names.contains(strItemName))
                names << strItemName;
            else
            {
                message.second << UIGlobalSettingsNetwork::tr("The name <b>%1</b> is being used for several NAT networks.")
                                                              .arg(strItemName);
                fPass = false;
            }
        }

        /* Serialize message: */
        if (!message.second.isEmpty())
            messages << message;
    }

    /* Validate Host network items: */
    {
        /* Prepare message: */
        UIValidationMessage message;
        message.first = VBoxGlobal::removeAccelMark(m_pTabWidget->tabText(1));

        /* Validate items only: */
        for (int i = 0; i < m_pTreeNetworkHost->topLevelItemCount(); ++i)
        {
            UIItemNetworkHost *pItem = static_cast<UIItemNetworkHost*>(m_pTreeNetworkHost->topLevelItem(i));
            if (!pItem->validate(message))
                fPass = false;
        }

        /* Serialize message: */
        if (!message.second.isEmpty())
            messages << message;
    }

    /* Return result: */
    return fPass;
}

void UIGlobalSettingsNetwork::setOrderAfter(QWidget *pWidget)
{
    setTabOrder(pWidget, m_pTabWidget->focusProxy());
    setTabOrder(m_pTabWidget->focusProxy(), m_pTreeNetworkNAT);
    setTabOrder(m_pTreeNetworkNAT, m_pTreeNetworkHost);
}

void UIGlobalSettingsNetwork::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsNetwork::retranslateUi(this);

    /* NAT networks: */
    {
        /* Translate tree-widget columns: */
        m_pTreeNetworkNAT->setHeaderLabels(QStringList()
                                           << tr("Active", "NAT network")
                                           << tr("Name"));

        /* Translate action text: */
        m_pActionAddNetworkNAT->setText(tr("Add NAT Network"));
        m_pActionRemoveNetworkNAT->setText(tr("Remove NAT Network"));
        m_pActionEditNetworkNAT->setText(tr("Edit NAT Network"));

        m_pActionAddNetworkNAT->setWhatsThis(tr("Adds new NAT network."));
        m_pActionRemoveNetworkNAT->setWhatsThis(tr("Removes selected NAT network."));
        m_pActionEditNetworkNAT->setWhatsThis(tr("Edits selected NAT network."));

        m_pActionAddNetworkNAT->setToolTip(m_pActionAddNetworkNAT->whatsThis());
        m_pActionRemoveNetworkNAT->setToolTip(m_pActionRemoveNetworkNAT->whatsThis());
        m_pActionEditNetworkNAT->setToolTip(m_pActionEditNetworkNAT->whatsThis());
    }

    /* Host networks: */
    {
        /* Translate action text: */
        m_pActionAddNetworkHost->setText(tr("Add Host-only Network"));
        m_pActionRemoveNetworkHost->setText(tr("Remove Host-only Network"));
        m_pActionEditNetworkHost->setText(tr("Edit Host-only Network"));

        m_pActionAddNetworkHost->setWhatsThis(tr("Adds new host-only network."));
        m_pActionRemoveNetworkHost->setWhatsThis(tr("Removes selected host-only network."));
        m_pActionEditNetworkHost->setWhatsThis(tr("Edits selected host-only network."));

        m_pActionAddNetworkHost->setToolTip(m_pActionAddNetworkHost->whatsThis());
        m_pActionRemoveNetworkHost->setToolTip(m_pActionRemoveNetworkHost->whatsThis());
        m_pActionEditNetworkHost->setToolTip(m_pActionEditNetworkHost->whatsThis());
    }
}

void UIGlobalSettingsNetwork::sltAddNetworkNAT()
{
    /* Compose a set of busy names: */
    QSet<QString> names;
    for (int i = 0; i < m_pTreeNetworkNAT->topLevelItemCount(); ++i)
        names << static_cast<UIItemNetworkNAT*>(m_pTreeNetworkNAT->topLevelItem(i))->name();
    /* Compose a map of busy indexes: */
    QMap<int, bool> presence;
    const QString strNameTemplate("NatNetwork%1");
    const QRegExp regExp(strNameTemplate.arg("([\\d]*)"));
    foreach (const QString &strName, names)
        if (regExp.indexIn(strName) != -1)
            presence[regExp.cap(1).toInt()] = true;
    /* Search for a minimum index: */
    int iMinimumIndex = 0;
    for (int i = 0; !presence.isEmpty() && i <= presence.lastKey() + 1; ++i)
        if (!presence.contains(i))
        {
            iMinimumIndex = i;
            break;
        }

    /* Compose resulting index and name: */
    const QString strNetworkIndex(iMinimumIndex == 0 ? QString() : QString::number(iMinimumIndex));
    const QString strNetworkName = strNameTemplate.arg(strNetworkIndex);

    /* Compose new item data: */
    UIDataSettingsGlobalNetworkNAT data;
    data.m_fEnabled = true;
    data.m_strName = strNetworkName;
    data.m_strNewName = strNetworkName;
    data.m_strCIDR = "10.0.2.0/24";
    data.m_fSupportsDHCP = true;

    /* Create tree-widget item: */
    createTreeWidgetItemForNetworkNAT(data, UIPortForwardingDataList(), UIPortForwardingDataList(), true);
    m_pTreeNetworkNAT->sortByColumn(1, Qt::AscendingOrder);
}

void UIGlobalSettingsNetwork::sltEditNetworkNAT()
{
    /* Get network item: */
    UIItemNetworkNAT *pItem = static_cast<UIItemNetworkNAT*>(m_pTreeNetworkNAT->currentItem());
    AssertPtrReturnVoid(pItem);

    /* Edit current item data: */
    UIDataSettingsGlobalNetworkNAT data = *pItem;
    UIPortForwardingDataList ipv4rules = pItem->ipv4rules();
    UIPortForwardingDataList ipv6rules = pItem->ipv6rules();
    UIGlobalSettingsNetworkDetailsNAT details(this, data, ipv4rules, ipv6rules);
    if (details.exec() == QDialog::Accepted)
    {
        /* Put data back: */
        pItem->UIDataSettingsGlobalNetworkNAT::operator=(data);
        pItem->setIpv4rules(ipv4rules);
        pItem->setIpv6rules(ipv6rules);
        pItem->updateFields();
        sltHandleCurrentItemChangeNetworkNAT();
        /* Revalidate: */
        revalidate();
    }
}

void UIGlobalSettingsNetwork::sltRemoveNetworkNAT()
{
    /* Get network item: */
    UIItemNetworkNAT *pItem = static_cast<UIItemNetworkNAT*>(m_pTreeNetworkNAT->currentItem());
    AssertPtrReturnVoid(pItem);

    /* Confirm network removal: */
    if (!msgCenter().confirmNATNetworkRemoval(pItem->name(), this))
        return;

    /* Remove tree-widget item: */
    removeTreeWidgetItemOfNetworkNAT(pItem);
}

void UIGlobalSettingsNetwork::sltHandleItemChangeNetworkNAT(QTreeWidgetItem *pChangedItem)
{
    /* Get network item: */
    UIItemNetworkNAT *pItem = static_cast<UIItemNetworkNAT*>(pChangedItem);
    AssertMsg(pItem, ("Changed item should present!\n"));

    /* Update item data: */
    pItem->updateData();
}

void UIGlobalSettingsNetwork::sltHandleCurrentItemChangeNetworkNAT()
{
    /* Get current item: */
    UIItemNetworkNAT *pItem = static_cast<UIItemNetworkNAT*>(m_pTreeNetworkNAT->currentItem());
    /* Update availability: */
    m_pActionRemoveNetworkNAT->setEnabled(pItem);
    m_pActionEditNetworkNAT->setEnabled(pItem);
}

void UIGlobalSettingsNetwork::sltHandleContextMenuRequestNetworkNAT(const QPoint &pos)
{
    /* Compose temporary context-menu: */
    QMenu menu;
    if (m_pTreeNetworkNAT->itemAt(pos))
    {
        menu.addAction(m_pActionEditNetworkNAT);
        menu.addAction(m_pActionRemoveNetworkNAT);
    }
    else
    {
        menu.addAction(m_pActionAddNetworkNAT);
    }
    /* And show it: */
    menu.exec(m_pTreeNetworkNAT->mapToGlobal(pos));
}

void UIGlobalSettingsNetwork::sltAddNetworkHost()
{
    /* Prepare useful variables: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CHost host = vboxGlobal().host();

    /* Create Host interface: */
    CHostNetworkInterface iface;
    CProgress progress = host.CreateHostOnlyNetworkInterface(iface);
    if (!host.isOk())
        return msgCenter().cannotCreateHostNetworkInterface(host, this);
    msgCenter().showModalProgressDialog(progress, tr("Networking"), ":/progress_network_interface_90px.png", this, 0);
    if (!progress.isOk() || progress.GetResultCode() != 0)
        return msgCenter().cannotCreateHostNetworkInterface(progress, this);

    /* Make sure DHCP server is created too: */
    CDHCPServer dhcp = vbox.FindDHCPServerByNetworkName(iface.GetNetworkName());
    if (dhcp.isNull())
    {
        vbox.CreateDHCPServer(iface.GetNetworkName());
        if (!vbox.isOk())
            return msgCenter().cannotCreateDHCPServer(vbox, iface.GetNetworkName(), this);
        dhcp = vbox.FindDHCPServerByNetworkName(iface.GetNetworkName());
    }
    AssertReturnVoid(!dhcp.isNull());

    /* Update tree: */
    const QString strCacheKey = iface.GetName();
    loadToCacheFromNetworkHost(iface, m_pCache->child2(strCacheKey));
    createTreeWidgetItemForNetworkHost(m_pCache->child2(strCacheKey), true);
    m_pTreeNetworkHost->sortByColumn(0, Qt::AscendingOrder);
}

void UIGlobalSettingsNetwork::sltEditNetworkHost()
{
    /* Get network item: */
    UIItemNetworkHost *pItem = static_cast<UIItemNetworkHost*>(m_pTreeNetworkHost->currentItem());
    AssertMsg(pItem, ("Current item should present!\n"));

    /* Edit current item data: */
    UIDataSettingsGlobalNetworkHost data = *pItem;
    UIGlobalSettingsNetworkDetailsHost details(this, data);
    if (details.exec() == QDialog::Accepted)
    {
        /* Put data back: */
        pItem->UIDataSettingsGlobalNetworkHost::operator=(data);
        pItem->updateFields();
        sltHandleCurrentItemChangeNetworkHost();
        /* Revalidate: */
        revalidate();
    }
}

void UIGlobalSettingsNetwork::sltRemoveNetworkHost()
{
    /* Get network item: */
    UIItemNetworkHost *pItem = static_cast<UIItemNetworkHost*>(m_pTreeNetworkHost->currentItem());
    AssertMsg(pItem, ("Current item should present!\n"));
    /* Get interface name: */
    const QString strInterfaceName(pItem->name());

    /* Confirm Host network removal: */
    if (!msgCenter().confirmHostOnlyInterfaceRemoval(strInterfaceName, this))
        return;

    /* Prepare useful variables: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CHost host = vboxGlobal().host();

    /* Find corresponding interface: */
    const CHostNetworkInterface &iface = host.FindHostNetworkInterfaceByName(strInterfaceName);
    AssertReturnVoid(host.isOk() && !iface.isNull());

    /* Remove DHCP server if any: */
    const CDHCPServer &dhcp = vbox.FindDHCPServerByNetworkName(iface.GetNetworkName());
    if (vbox.isOk() && !dhcp.isNull())
    {
        vbox.RemoveDHCPServer(dhcp);
        if (!vbox.isOk())
            return msgCenter().cannotRemoveDHCPServer(vbox, strInterfaceName, this);
    }

    /* Remove interface finally: */
    CProgress progress = host.RemoveHostOnlyNetworkInterface(iface.GetId());
    if (!host.isOk())
        return msgCenter().cannotRemoveHostNetworkInterface(host, strInterfaceName, this);
    msgCenter().showModalProgressDialog(progress, tr("Networking"), ":/progress_network_interface_90px.png", this, 0);
    if (!progress.isOk() || progress.GetResultCode() != 0)
        return msgCenter().cannotRemoveHostNetworkInterface(progress, strInterfaceName, this);

    /* Update tree: */
    removeTreeWidgetItemOfNetworkHost(pItem);
}

void UIGlobalSettingsNetwork::sltHandleCurrentItemChangeNetworkHost()
{
    /* Get current item: */
    UIItemNetworkHost *pItem = static_cast<UIItemNetworkHost*>(m_pTreeNetworkHost->currentItem());
    /* Update availability: */
    m_pActionRemoveNetworkHost->setEnabled(pItem);
    m_pActionEditNetworkHost->setEnabled(pItem);
}

void UIGlobalSettingsNetwork::sltHandleContextMenuRequestNetworkHost(const QPoint &pos)
{
    /* Compose temporary context-menu: */
    QMenu menu;
    if (m_pTreeNetworkHost->itemAt(pos))
    {
        menu.addAction(m_pActionEditNetworkHost);
        menu.addAction(m_pActionRemoveNetworkHost);
    }
    else
    {
        menu.addAction(m_pActionAddNetworkHost);
    }
    /* And show it: */
    menu.exec(m_pTreeNetworkHost->mapToGlobal(pos));
}

void UIGlobalSettingsNetwork::prepare()
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsNetwork::setupUi(this);

    /* Prepare cache: */
    m_pCache = new UISettingsCacheGlobalNetwork;
    AssertPtrReturnVoid(m_pCache);

    /* Tree-widget created in the .ui file. */
    {
        /* Prepare 'NAT Network' tab: */
        prepareTabNAT();
        /* Prepare 'Host Network' tab: */
        prepareTabHost();
        /* Prepare connections: */
        prepareConnections();
    }

    /* Apply language settings: */
    retranslateUi();
}

void UIGlobalSettingsNetwork::prepareTabNAT()
{
    /* Tab and it's layout created in the .ui file. */
    {
#ifdef VBOX_WS_MAC
        /* On macOS we can do a bit of smoothness: */
        m_pLayoutNAT->setContentsMargins(0, 0, 0, 0);
#endif

        /* NAT Network tree-widget created in the .ui file. */
        AssertPtrReturnVoid(m_pTreeNetworkNAT);
        {
            /* Configure tree-widget: */
            m_pTreeNetworkNAT->setColumnCount(2);
            m_pTreeNetworkNAT->header()->setStretchLastSection(false);
            m_pTreeNetworkNAT->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
            m_pTreeNetworkNAT->header()->setSectionResizeMode(1, QHeaderView::Stretch);
            m_pTreeNetworkNAT->setContextMenuPolicy(Qt::CustomContextMenu);
        }

        /* NAT Network toolbar created in the .ui file. */
        AssertPtrReturnVoid(m_pToolbarNetworkNAT);
        {
            /* Configure toolbar: */
            const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
            m_pToolbarNetworkNAT->setIconSize(QSize(iIconMetric, iIconMetric));
            m_pToolbarNetworkNAT->setOrientation(Qt::Vertical);

            /* Create' Add NAT Network' action: */
            m_pActionAddNetworkNAT = m_pToolbarNetworkNAT->addAction(UIIconPool::iconSet(":/add_host_iface_16px.png",
                                                                                         ":/add_host_iface_disabled_16px.png"),
                                                                     QString(), this, SLOT(sltAddNetworkNAT()));
            AssertPtrReturnVoid(m_pActionAddNetworkNAT);
            {
                /* Configure action: */
                m_pActionAddNetworkNAT->setShortcuts(QList<QKeySequence>() << QKeySequence("Ins") << QKeySequence("Ctrl+N"));
            }

            /* Create 'Edit NAT Network' action: */
            m_pActionEditNetworkNAT = m_pToolbarNetworkNAT->addAction(UIIconPool::iconSet(":/edit_host_iface_16px.png",
                                                                                          ":/edit_host_iface_disabled_16px.png"),
                                                                      QString(), this, SLOT(sltEditNetworkNAT()));
            AssertPtrReturnVoid(m_pActionEditNetworkNAT);
            {
                /* Configure action: */
                m_pActionEditNetworkNAT->setShortcuts(QList<QKeySequence>() << QKeySequence("Space") << QKeySequence("F2"));
            }

            /* Create 'Remove NAT Network' action: */
            m_pActionRemoveNetworkNAT = m_pToolbarNetworkNAT->addAction(UIIconPool::iconSet(":/remove_host_iface_16px.png",
                                                                                            ":/remove_host_iface_disabled_16px.png"),
                                                                        QString(), this, SLOT(sltRemoveNetworkNAT()));
            AssertPtrReturnVoid(m_pActionRemoveNetworkNAT);
            {
                /* Configure action: */
                m_pActionRemoveNetworkNAT->setShortcuts(QList<QKeySequence>() << QKeySequence("Del") << QKeySequence("Ctrl+R"));
            }
        }
    }
}

void UIGlobalSettingsNetwork::prepareTabHost()
{
    /* Tab and it's layout created in the .ui file. */
    {
#ifdef VBOX_WS_MAC
        /* On macOS we can do a bit of smoothness: */
        m_pLayoutHostOnly->setContentsMargins(0, 0, 0, 0);
#endif

        /* Host Network tree-widget created in the .ui file. */
        AssertPtrReturnVoid(m_pTreeNetworkHost);
        {
            /* Configure tree-widget: */
            m_pTreeNetworkHost->header()->hide();
            m_pTreeNetworkHost->setContextMenuPolicy(Qt::CustomContextMenu);
        }

        /* Host Network toolbar created in the .ui file. */
        AssertPtrReturnVoid(m_pToolbarNetworkHost);
        {
            /* Configure toolbar: */
            const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
            m_pToolbarNetworkHost->setIconSize(QSize(iIconMetric, iIconMetric));
            m_pToolbarNetworkHost->setOrientation(Qt::Vertical);

            /* Create 'Add Host Network' action: */
            m_pActionAddNetworkHost = m_pToolbarNetworkHost->addAction(UIIconPool::iconSet(":/add_host_iface_16px.png",
                                                                                           ":/add_host_iface_disabled_16px.png"),
                                                                       QString(), this, SLOT(sltAddNetworkHost()));
            AssertPtrReturnVoid(m_pActionAddNetworkHost);
            {
                /* Configure action: */
                m_pActionAddNetworkHost->setShortcuts(QList<QKeySequence>() << QKeySequence("Ins") << QKeySequence("Ctrl+N"));
            }

            /* Create 'Edit Host Network' action: */
            m_pActionEditNetworkHost = m_pToolbarNetworkHost->addAction(UIIconPool::iconSet(":/edit_host_iface_16px.png",
                                                                                            ":/edit_host_iface_disabled_16px.png"),
                                                                        QString(), this, SLOT(sltEditNetworkHost()));
            AssertPtrReturnVoid(m_pActionEditNetworkHost);
            {
                /* Configure action: */
                m_pActionEditNetworkHost->setShortcuts(QList<QKeySequence>() << QKeySequence("Space") << QKeySequence("F2"));
            }

            /* Create 'Remove Host Network' action: */
            m_pActionRemoveNetworkHost = m_pToolbarNetworkHost->addAction(UIIconPool::iconSet(":/remove_host_iface_16px.png",
                                                                                              ":/remove_host_iface_disabled_16px.png"),
                                                                          QString(), this, SLOT(sltRemoveNetworkHost()));
            AssertPtrReturnVoid(m_pActionRemoveNetworkHost);
            {
                /* Configure action: */
                m_pActionRemoveNetworkHost->setShortcuts(QList<QKeySequence>() << QKeySequence("Del") << QKeySequence("Ctrl+R"));
            }
        }
    }
}

void UIGlobalSettingsNetwork::prepareConnections()
{
    /* Configure 'NAT Network' connections: */
    connect(m_pTreeNetworkNAT, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
            this, SLOT(sltHandleCurrentItemChangeNetworkNAT()));
    connect(m_pTreeNetworkNAT, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(sltHandleContextMenuRequestNetworkNAT(const QPoint &)));
    connect(m_pTreeNetworkNAT, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
            this, SLOT(sltEditNetworkNAT()));
    connect(m_pTreeNetworkNAT, SIGNAL(itemChanged(QTreeWidgetItem *, int)),
            this, SLOT(sltHandleItemChangeNetworkNAT(QTreeWidgetItem *)));

    /* Configure 'Host Network' connections: */
    connect(m_pTreeNetworkHost, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
            this, SLOT(sltHandleCurrentItemChangeNetworkHost()));
    connect(m_pTreeNetworkHost, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(sltHandleContextMenuRequestNetworkHost(const QPoint &)));
    connect(m_pTreeNetworkHost, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
            this, SLOT(sltEditNetworkHost()));
}

void UIGlobalSettingsNetwork::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

bool UIGlobalSettingsNetwork::saveNetworkData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save network settings from the cache: */
    if (fSuccess && m_pCache->wasChanged())
    {
        /* For each NAT network ('removing' step): */
        // We need to separately remove NAT networks first because
        // there could be name collisions with the existing NAT networks.
        for (int i = 0; fSuccess && i < m_pCache->childCount1(); ++i)
        {
            /* Get NAT network cache: */
            const UISettingsCacheGlobalNetworkNAT &cache = m_pCache->child1(i);

            /* Remove NAT network marked for 'remove' or 'update' (if it can't be updated): */
            if (cache.wasRemoved() || (cache.wasUpdated() && !isNetworkCouldBeUpdated(cache)))
                fSuccess = removeNetworkNAT(cache);
        }
        /* For each NAT network ('creating' step): */
        for (int i = 0; fSuccess && i < m_pCache->childCount1(); ++i)
        {
            /* Get NAT network cache: */
            const UISettingsCacheGlobalNetworkNAT &cache = m_pCache->child1(i);

            /* Create NAT network marked for 'create' or 'update' (if it can't be updated): */
            if (cache.wasCreated() || (cache.wasUpdated() && !isNetworkCouldBeUpdated(cache)))
                fSuccess = createNetworkNAT(cache);

            else

            /* Update NAT network marked for 'update' (if it can be updated): */
            if (cache.wasUpdated() && isNetworkCouldBeUpdated(cache))
                fSuccess = updateNetworkNAT(cache);
        }

        /* For each Host network ('updating' step): */
        for (int i = 0; fSuccess && i < m_pCache->childCount2(); ++i)
        {
            const UISettingsCacheGlobalNetworkHost &cache = m_pCache->child2(i);
            if (cache.wasUpdated())
                fSuccess = saveDataNetworkHost(m_pCache->child2(i));
        }
    }
    /* Return result: */
    return fSuccess;
}

void UIGlobalSettingsNetwork::loadToCacheFromNetworkNAT(const CNATNetwork &network, UISettingsCacheGlobalNetworkNAT &cache)
{
    /* Prepare old NAT data: */
    UIDataSettingsGlobalNetworkNAT oldNATData;

    /* Load NAT network settings: */
    oldNATData.m_fEnabled = network.GetEnabled();
    oldNATData.m_strName = network.GetNetworkName();
    oldNATData.m_strNewName = oldNATData.m_strName;
    oldNATData.m_strCIDR = network.GetNetwork();
    oldNATData.m_fSupportsDHCP = network.GetNeedDhcpServer();
    oldNATData.m_fSupportsIPv6 = network.GetIPv6Enabled();
    oldNATData.m_fAdvertiseDefaultIPv6Route = network.GetAdvertiseDefaultIPv6RouteEnabled();

    /* Load IPv4 rules: */
    foreach (QString strIPv4Rule, network.GetPortForwardRules4())
    {
        /* Replace all ':' with ',' first: */
        strIPv4Rule.replace(':', ',');
        /* Parse rules: */
        QStringList rules = strIPv4Rule.split(',');
        Assert(rules.size() == 6);
        if (rules.size() != 6)
            continue;
        cache.child1(rules.at(0)).cacheInitialData(UIDataPortForwardingRule(rules.at(0),
                                                                            gpConverter->fromInternalString<KNATProtocol>(rules.at(1)),
                                                                            QString(rules.at(2)).remove('[').remove(']'),
                                                                            rules.at(3).toUInt(),
                                                                            QString(rules.at(4)).remove('[').remove(']'),
                                                                            rules.at(5).toUInt()));
    }

    /* Load IPv6 rules: */
    foreach (QString strIPv6Rule, network.GetPortForwardRules6())
    {
        /* Replace all ':' with ',' first: */
        strIPv6Rule.replace(':', ',');
        /* But replace ',' back with ':' for addresses: */
        QRegExp re("\\[[0-9a-fA-F,]*,[0-9a-fA-F,]*\\]");
        re.setMinimal(true);
        while (re.indexIn(strIPv6Rule) != -1)
        {
            QString strCapOld = re.cap(0);
            QString strCapNew = strCapOld;
            strCapNew.replace(',', ':');
            strIPv6Rule.replace(strCapOld, strCapNew);
        }
        /* Parse rules: */
        QStringList rules = strIPv6Rule.split(',');
        Assert(rules.size() == 6);
        if (rules.size() != 6)
            continue;
        cache.child2(rules.at(0)).cacheInitialData(UIDataPortForwardingRule(rules.at(0),
                                                                            gpConverter->fromInternalString<KNATProtocol>(rules.at(1)),
                                                                            QString(rules.at(2)).remove('[').remove(']'),
                                                                            rules.at(3).toUInt(),
                                                                            QString(rules.at(4)).remove('[').remove(']'),
                                                                            rules.at(5).toUInt()));
    }

    /* Cache old NAT data: */
    cache.cacheInitialData(oldNATData);
}

bool UIGlobalSettingsNetwork::removeNetworkNAT(const UISettingsCacheGlobalNetworkNAT &cache)
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save NAT settings from the cache: */
    if (fSuccess)
    {
        /* Get old NAT data from the cache: */
        const UIDataSettingsGlobalNetworkNAT &oldNatData = cache.base();
        /* Get new NAT data from the cache: */
        //const UIDataSettingsGlobalNetworkNAT &newNatData = cache.data();

        /* Get VBox for further activities: */
        CVirtualBox comVBox = vboxGlobal().virtualBox();
        /* Search for a NAT network with required name: */
        CNATNetwork comNetwork = comVBox.FindNATNetworkByName(oldNatData.m_strName);
        fSuccess = comVBox.isOk() && comNetwork.isNotNull();

        /* Remove NAT network: */
        if (fSuccess)
        {
            comVBox.RemoveNATNetwork(comNetwork);
            fSuccess = comVBox.isOk();
        }

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIMessageCenter::formatErrorInfo(comVBox));
    }
    /* Return result: */
    return fSuccess;
}

bool UIGlobalSettingsNetwork::createNetworkNAT(const UISettingsCacheGlobalNetworkNAT &cache)
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save NAT settings from the cache: */
    if (fSuccess)
    {
        /* Get old NAT data from the cache: */
        //const UIDataSettingsGlobalNetworkNAT &oldNatData = cache.base();
        /* Get new NAT data from the cache: */
        const UIDataSettingsGlobalNetworkNAT &newNatData = cache.data();

        /* Get VBox for further activities: */
        CVirtualBox comVBox = vboxGlobal().virtualBox();
        /* Create NAT network with required name: */
        CNATNetwork comNetwork = comVBox.CreateNATNetwork(newNatData.m_strNewName);
        fSuccess = comVBox.isOk() && comNetwork.isNotNull();

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIMessageCenter::formatErrorInfo(comVBox));
        else
        {
            /* Save whether NAT network is enabled: */
            if (fSuccess)
            {
                comNetwork.SetEnabled(newNatData.m_fEnabled);
                fSuccess = comNetwork.isOk();
            }
            /* Save NAT network name: */
            if (fSuccess)
            {
                comNetwork.SetNetworkName(newNatData.m_strNewName);
                fSuccess = comNetwork.isOk();
            }
            /* Save NAT network CIDR: */
            if (fSuccess)
            {
                comNetwork.SetNetwork(newNatData.m_strCIDR);
                fSuccess = comNetwork.isOk();
            }
            /* Save whether NAT network needs DHCP server: */
            if (fSuccess)
            {
                comNetwork.SetNeedDhcpServer(newNatData.m_fSupportsDHCP);
                fSuccess = comNetwork.isOk();
            }
            /* Save whether NAT network supports IPv6: */
            if (fSuccess)
            {
                comNetwork.SetIPv6Enabled(newNatData.m_fSupportsIPv6);
                fSuccess = comNetwork.isOk();
            }
            /* Save whether NAT network should advertise default IPv6 route: */
            if (fSuccess)
            {
                comNetwork.SetAdvertiseDefaultIPv6RouteEnabled(newNatData.m_fAdvertiseDefaultIPv6Route);
                fSuccess = comNetwork.isOk();
            }

            /* Save IPv4 forwarding rules: */
            for (int i = 0; fSuccess && i < cache.childCount1(); ++i)
            {
                /* Get rule cache: */
                const UISettingsCachePortForwardingRule &ruleCache = cache.child1(i);

                /* Create rule not marked for 'remove': */
                if (!ruleCache.wasRemoved())
                {
                    comNetwork.AddPortForwardRule(false,
                                                  ruleCache.data().name, ruleCache.data().protocol,
                                                  ruleCache.data().hostIp, ruleCache.data().hostPort.value(),
                                                  ruleCache.data().guestIp, ruleCache.data().guestPort.value());
                    fSuccess = comNetwork.isOk();
                }
            }

            /* Save IPv6 forwarding rules: */
            for (int i = 0; fSuccess && i < cache.childCount2(); ++i)
            {
                /* Get rule cache: */
                const UISettingsCachePortForwardingRule &ruleCache = cache.child2(i);

                /* Create rule not marked for 'remove': */
                if (!ruleCache.wasRemoved())
                {
                    comNetwork.AddPortForwardRule(true,
                                                  ruleCache.data().name, ruleCache.data().protocol,
                                                  ruleCache.data().hostIp, ruleCache.data().hostPort.value(),
                                                  ruleCache.data().guestIp, ruleCache.data().guestPort.value());
                    fSuccess = comNetwork.isOk();
                }
            }

            /* Show error message if necessary: */
            if (!fSuccess)
                notifyOperationProgressError(UIMessageCenter::formatErrorInfo(comNetwork));
        }
    }
    /* Return result: */
    return fSuccess;
}

bool UIGlobalSettingsNetwork::updateNetworkNAT(const UISettingsCacheGlobalNetworkNAT &cache)
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save NAT settings from the cache: */
    if (fSuccess)
    {
        /* Get old NAT data from the cache: */
        const UIDataSettingsGlobalNetworkNAT &oldNatData = cache.base();
        /* Get new NAT data from the cache: */
        const UIDataSettingsGlobalNetworkNAT &newNatData = cache.data();

        /* Get VBox for further activities: */
        CVirtualBox comVBox = vboxGlobal().virtualBox();
        /* Search for a NAT network with required name: */
        CNATNetwork comNetwork = comVBox.FindNATNetworkByName(oldNatData.m_strName);
        fSuccess = comVBox.isOk() && comNetwork.isNotNull();

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIMessageCenter::formatErrorInfo(comVBox));
        else
        {
            /* Save whether NAT network is enabled: */
            if (fSuccess && newNatData.m_fEnabled != oldNatData.m_fEnabled)
            {
                comNetwork.SetEnabled(newNatData.m_fEnabled);
                fSuccess = comNetwork.isOk();
            }
            /* Save NAT network name: */
            if (fSuccess && newNatData.m_strNewName != oldNatData.m_strNewName)
            {
                comNetwork.SetNetworkName(newNatData.m_strNewName);
                fSuccess = comNetwork.isOk();
            }
            /* Save NAT network CIDR: */
            if (fSuccess && newNatData.m_strCIDR != oldNatData.m_strCIDR)
            {
                comNetwork.SetNetwork(newNatData.m_strCIDR);
                fSuccess = comNetwork.isOk();
            }
            /* Save whether NAT network needs DHCP server: */
            if (fSuccess && newNatData.m_fSupportsDHCP != oldNatData.m_fSupportsDHCP)
            {
                comNetwork.SetNeedDhcpServer(newNatData.m_fSupportsDHCP);
                fSuccess = comNetwork.isOk();
            }
            /* Save whether NAT network supports IPv6: */
            if (fSuccess && newNatData.m_fSupportsIPv6 != oldNatData.m_fSupportsIPv6)
            {
                comNetwork.SetIPv6Enabled(newNatData.m_fSupportsIPv6);
                fSuccess = comNetwork.isOk();
            }
            /* Save whether NAT network should advertise default IPv6 route: */
            if (fSuccess && newNatData.m_fAdvertiseDefaultIPv6Route != oldNatData.m_fAdvertiseDefaultIPv6Route)
            {
                comNetwork.SetAdvertiseDefaultIPv6RouteEnabled(newNatData.m_fAdvertiseDefaultIPv6Route);
                fSuccess = comNetwork.isOk();
            }

            /* Save IPv4 forwarding rules: */
            for (int i = 0; fSuccess && i < cache.childCount1(); ++i)
            {
                /* Get rule cache: */
                const UISettingsCachePortForwardingRule &ruleCache = cache.child1(i);

                /* Remove rule marked for 'remove' or 'update': */
                if (ruleCache.wasRemoved() || ruleCache.wasUpdated())
                {
                    comNetwork.RemovePortForwardRule(false,
                                                     ruleCache.base().name);
                    fSuccess = comNetwork.isOk();
                }
            }
            for (int i = 0; fSuccess && i < cache.childCount1(); ++i)
            {
                /* Get rule cache: */
                const UISettingsCachePortForwardingRule &ruleCache = cache.child1(i);

                /* Create rule marked for 'create' or 'update': */
                if (ruleCache.wasCreated() || ruleCache.wasUpdated())
                {
                    comNetwork.AddPortForwardRule(false,
                                                  ruleCache.data().name, ruleCache.data().protocol,
                                                  ruleCache.data().hostIp, ruleCache.data().hostPort.value(),
                                                  ruleCache.data().guestIp, ruleCache.data().guestPort.value());
                    fSuccess = comNetwork.isOk();
                }
            }

            /* Save IPv6 forwarding rules: */
            for (int i = 0; fSuccess && i < cache.childCount2(); ++i)
            {
                /* Get rule cache: */
                const UISettingsCachePortForwardingRule &ruleCache = cache.child2(i);

                /* Remove rule marked for 'remove' or 'update': */
                if (ruleCache.wasRemoved() || ruleCache.wasUpdated())
                {
                    comNetwork.RemovePortForwardRule(true,
                                                     ruleCache.base().name);
                    fSuccess = comNetwork.isOk();
                }
            }
            for (int i = 0; fSuccess && i < cache.childCount2(); ++i)
            {
                /* Get rule cache: */
                const UISettingsCachePortForwardingRule &ruleCache = cache.child2(i);

                /* Create rule marked for 'create' or 'update': */
                if (ruleCache.wasCreated() || ruleCache.wasUpdated())
                {
                    comNetwork.AddPortForwardRule(true,
                                                  ruleCache.data().name, ruleCache.data().protocol,
                                                  ruleCache.data().hostIp, ruleCache.data().hostPort.value(),
                                                  ruleCache.data().guestIp, ruleCache.data().guestPort.value());
                    fSuccess = comNetwork.isOk();
                }
            }

            /* Show error message if necessary: */
            if (!fSuccess)
                notifyOperationProgressError(UIMessageCenter::formatErrorInfo(comNetwork));
        }
    }
    /* Return result: */
    return fSuccess;
}

void UIGlobalSettingsNetwork::createTreeWidgetItemForNetworkNAT(const UISettingsCacheGlobalNetworkNAT &cache)
{
    /* Get old NAT data: */
    const UIDataSettingsGlobalNetworkNAT oldNATData = cache.base();

    /* Load old port forwarding rules: */
    UIPortForwardingDataList ipv4rules;
    UIPortForwardingDataList ipv6rules;
    for (int i = 0; i < cache.childCount1(); ++i)
        ipv4rules << cache.child1(i).base();
    for (int i = 0; i < cache.childCount2(); ++i)
        ipv6rules << cache.child2(i).base();

    /* Pass to wrapper below: */
    createTreeWidgetItemForNetworkNAT(oldNATData, ipv4rules, ipv6rules, false /* choose item? */);
}

void UIGlobalSettingsNetwork::createTreeWidgetItemForNetworkNAT(const UIDataSettingsGlobalNetworkNAT &data,
                                                                const UIPortForwardingDataList &ipv4rules,
                                                                const UIPortForwardingDataList &ipv6rules,
                                                                bool fChooseItem /* = false */)
{
    /* Create tree-widget item: */
    UIItemNetworkNAT *pItem = new UIItemNetworkNAT;
    AssertPtrReturnVoid(pItem);
    {
        /* Configure item: */
        pItem->UIDataSettingsGlobalNetworkNAT::operator=(data);
        pItem->setIpv4rules(ipv4rules);
        pItem->setIpv6rules(ipv6rules);
        pItem->updateFields();

        /* Add item to the tree-widget: */
        m_pTreeNetworkNAT->addTopLevelItem(pItem);
    }

    /* And choose it as current if necessary: */
    if (fChooseItem)
        m_pTreeNetworkNAT->setCurrentItem(pItem);
}

void UIGlobalSettingsNetwork::removeTreeWidgetItemOfNetworkNAT(UIItemNetworkNAT *pItem)
{
    /* Delete passed item: */
    delete pItem;
}

bool UIGlobalSettingsNetwork::isNetworkCouldBeUpdated(const UISettingsCacheGlobalNetworkNAT &cache) const
{
    /* INATNetwork interface allows to set 'name' attribute
     * which can conflict with another one NAT network name.
     * This attribute could be changed in GUI directly or indirectly.
     * For such cases we have to recreate INATNetwork instance,
     * for other cases we will update NAT network attributes only. */
    const UIDataSettingsGlobalNetworkNAT &oldNATData = cache.base();
    const UIDataSettingsGlobalNetworkNAT &newNATData = cache.data();
    return true
           && (newNATData.m_strNewName == oldNATData.m_strNewName)
           ;
}

void UIGlobalSettingsNetwork::loadToCacheFromNetworkHost(const CHostNetworkInterface &iface, UISettingsCacheGlobalNetworkHost &cache)
{
    /* Prepare old NAT data: */
    UIDataSettingsGlobalNetworkHost oldHostData;

    /* Get DHCP server (create if necessary): */
    CDHCPServer dhcp = vboxGlobal().virtualBox().FindDHCPServerByNetworkName(iface.GetNetworkName());
    if (dhcp.isNull())
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        vbox.CreateDHCPServer(iface.GetNetworkName());
        if (!vbox.isOk())
        {
            msgCenter().cannotCreateDHCPServer(vbox, iface.GetNetworkName(), this);
            return;
        }
        dhcp = vboxGlobal().virtualBox().FindDHCPServerByNetworkName(iface.GetNetworkName());
    }
    Assert(!dhcp.isNull());
    if (dhcp.isNull())
        return;

    /* Host interface settings: */
    oldHostData.m_interface.m_strName = iface.GetName();
    oldHostData.m_interface.m_fDhcpClientEnabled = iface.GetDHCPEnabled();
    oldHostData.m_interface.m_strInterfaceAddress = iface.GetIPAddress();
    oldHostData.m_interface.m_strInterfaceMask = iface.GetNetworkMask();
    oldHostData.m_interface.m_fIpv6Supported = iface.GetIPV6Supported();
    oldHostData.m_interface.m_strInterfaceAddress6 = iface.GetIPV6Address();
    oldHostData.m_interface.m_strInterfaceMaskLength6 = QString::number(iface.GetIPV6NetworkMaskPrefixLength());

    /* DHCP server settings: */
    oldHostData.m_dhcpserver.m_fDhcpServerEnabled = dhcp.GetEnabled();
    oldHostData.m_dhcpserver.m_strDhcpServerAddress = dhcp.GetIPAddress();
    oldHostData.m_dhcpserver.m_strDhcpServerMask = dhcp.GetNetworkMask();
    oldHostData.m_dhcpserver.m_strDhcpLowerAddress = dhcp.GetLowerIP();
    oldHostData.m_dhcpserver.m_strDhcpUpperAddress = dhcp.GetUpperIP();

    /* Cache old NAT data: */
    cache.cacheInitialData(oldHostData);
}

bool UIGlobalSettingsNetwork::saveDataNetworkHost(const UISettingsCacheGlobalNetworkHost &cache)
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save host-only settings from the cache: */
    if (fSuccess)
    {
        /* Get old host-only data from the cache: */
        const UIDataSettingsGlobalNetworkHost &oldHostOnlyData = cache.base();
        /* Get new host-only data from the cache: */
        const UIDataSettingsGlobalNetworkHost &newHostOnlyData = cache.data();

        /* Get VBox for further activities: */
        CHost comHost = vboxGlobal().host();
        /* Search for a host-only interface with the same name: */
        CHostNetworkInterface comInterface = comHost.FindHostNetworkInterfaceByName(newHostOnlyData.m_interface.m_strName);
        fSuccess = comHost.isOk() && comInterface.isNotNull();

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIMessageCenter::formatErrorInfo(comHost));
        else
        {
            /* Automatic host interface configuration: */
            if (   fSuccess
                && newHostOnlyData.m_interface.m_fDhcpClientEnabled
                && newHostOnlyData.m_interface.m_fDhcpClientEnabled != oldHostOnlyData.m_interface.m_fDhcpClientEnabled)
            {
                comInterface.EnableDynamicIPConfig();
                fSuccess = comInterface.isOk();
            }

            else

            /* Manual host interface configuration: */
            if (   fSuccess
                && !newHostOnlyData.m_interface.m_fDhcpClientEnabled)
            {
                if (   fSuccess
                    && (   newHostOnlyData.m_interface.m_fDhcpClientEnabled != oldHostOnlyData.m_interface.m_fDhcpClientEnabled
                        || newHostOnlyData.m_interface.m_strInterfaceAddress != oldHostOnlyData.m_interface.m_strInterfaceAddress
                        || newHostOnlyData.m_interface.m_strInterfaceMask != oldHostOnlyData.m_interface.m_strInterfaceMask))
                {
                    comInterface.EnableStaticIPConfig(newHostOnlyData.m_interface.m_strInterfaceAddress, newHostOnlyData.m_interface.m_strInterfaceMask);
                    fSuccess = comInterface.isOk();
                }

                if (   fSuccess
                    && newHostOnlyData.m_interface.m_fIpv6Supported
                    && (   newHostOnlyData.m_interface.m_fDhcpClientEnabled != oldHostOnlyData.m_interface.m_fDhcpClientEnabled
                        || newHostOnlyData.m_interface.m_strInterfaceAddress6 != oldHostOnlyData.m_interface.m_strInterfaceAddress6
                        || newHostOnlyData.m_interface.m_strInterfaceMaskLength6 != oldHostOnlyData.m_interface.m_strInterfaceMaskLength6))
                {
                    comInterface.EnableStaticIPConfigV6(newHostOnlyData.m_interface.m_strInterfaceAddress6, newHostOnlyData.m_interface.m_strInterfaceMaskLength6.toULong());
                    fSuccess = comInterface.isOk();
                }
            }

            /* Get network name for further activities: */
            QString strNetworkName;
            if (fSuccess)
            {
                strNetworkName = comInterface.GetNetworkName();
                fSuccess = comInterface.isOk();
            }

            /* Show error message if necessary: */
            if (!fSuccess)
                notifyOperationProgressError(UIMessageCenter::formatErrorInfo(comInterface));
            else
            {
                /* Get VBox for further activities: */
                CVirtualBox comVBox = vboxGlobal().virtualBox();
                /* Search for a DHCP server with the same name: */
                CDHCPServer comServer = comVBox.FindDHCPServerByNetworkName(strNetworkName);
                fSuccess = comVBox.isOk() && comServer.isNotNull();

                /* Show error message if necessary: */
                if (!fSuccess)
                    notifyOperationProgressError(UIMessageCenter::formatErrorInfo(comVBox));
                else
                {
                    /* Save whether DHCP server is enabled: */
                    if (fSuccess && newHostOnlyData.m_dhcpserver.m_fDhcpServerEnabled != oldHostOnlyData.m_dhcpserver.m_fDhcpServerEnabled)
                    {
                        comServer.SetEnabled(newHostOnlyData.m_dhcpserver.m_fDhcpServerEnabled);
                        fSuccess = comServer.isOk();
                    }
                    if (   fSuccess
                        && newHostOnlyData.m_dhcpserver.m_fDhcpServerEnabled
                        && (   newHostOnlyData.m_dhcpserver.m_strDhcpServerAddress != oldHostOnlyData.m_dhcpserver.m_strDhcpServerAddress
                            || newHostOnlyData.m_dhcpserver.m_strDhcpServerMask != oldHostOnlyData.m_dhcpserver.m_strDhcpServerMask
                            || newHostOnlyData.m_dhcpserver.m_strDhcpLowerAddress != oldHostOnlyData.m_dhcpserver.m_strDhcpLowerAddress
                            || newHostOnlyData.m_dhcpserver.m_strDhcpUpperAddress != oldHostOnlyData.m_dhcpserver.m_strDhcpUpperAddress))
                    {
                        comServer.SetConfiguration(newHostOnlyData.m_dhcpserver.m_strDhcpServerAddress, newHostOnlyData.m_dhcpserver.m_strDhcpServerMask,
                                                   newHostOnlyData.m_dhcpserver.m_strDhcpLowerAddress, newHostOnlyData.m_dhcpserver.m_strDhcpUpperAddress);
                        fSuccess = comServer.isOk();
                    }

                    /* Show error message if necessary: */
                    if (!fSuccess)
                        notifyOperationProgressError(UIMessageCenter::formatErrorInfo(comServer));
                }
            }
        }
    }
    /* Return result: */
    return fSuccess;
}

void UIGlobalSettingsNetwork::createTreeWidgetItemForNetworkHost(const UISettingsCacheGlobalNetworkHost &cache, bool fChooseItem)
{
    /* Add new item to the tree: */
    UIItemNetworkHost *pItem = new UIItemNetworkHost;
    pItem->UIDataSettingsGlobalNetworkHost::operator=(cache.base());
    pItem->updateFields();
    m_pTreeNetworkHost->addTopLevelItem(pItem);
    /* And choose it as current if necessary: */
    if (fChooseItem)
        m_pTreeNetworkHost->setCurrentItem(pItem);
}

void UIGlobalSettingsNetwork::removeTreeWidgetItemOfNetworkHost(UIItemNetworkHost *pItem)
{
    /* Delete passed item: */
    delete pItem;
}


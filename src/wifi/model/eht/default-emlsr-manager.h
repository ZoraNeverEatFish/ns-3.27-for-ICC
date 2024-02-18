/*
 * Copyright (c) 2023 Universita' degli Studi di Napoli Federico II
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Stefano Avallone <stavallo@unina.it>
 */

#ifndef DEFAULT_EMLSR_MANAGER_H
#define DEFAULT_EMLSR_MANAGER_H

#include "emlsr-manager.h"

#include <optional>

namespace ns3
{

/**
 * \ingroup wifi
 *
 * DefaultEmlsrManager is the default EMLSR manager.
 */
class DefaultEmlsrManager : public EmlsrManager
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    DefaultEmlsrManager();
    ~DefaultEmlsrManager() override;

    bool SwitchMainPhyIfTxopGainedByAuxPhy(uint8_t linkId) override;

    /**
     * \param linkId the ID of the link on which TXOP is gained
     * \return zero, indicating that the TXOP can be started
     */
    Time GetDelayUntilAccessRequest(uint8_t linkId) override;

  protected:
    uint8_t GetLinkToSendEmlOmn() override;
    std::optional<uint8_t> ResendNotification(Ptr<const WifiMpdu> mpdu) override;

  private:
    void DoNotifyMgtFrameReceived(Ptr<const WifiMpdu> mpdu, uint8_t linkId) override;
    void NotifyEmlsrModeChanged() override;
    void NotifyMainPhySwitch(uint8_t currLinkId, uint8_t nextLinkId, Time duration) override;
    void DoNotifyIcfReceived(uint8_t linkId) override;
    void DoNotifyUlTxopStart(uint8_t linkId) override;
    void DoNotifyTxopEnd(uint8_t linkId) override;

    bool m_switchAuxPhy;  /**< whether Aux PHY should switch channel to operate on the link on which
                               the Main PHY was operating before moving to the link of the Aux PHY */
    bool m_auxPhyToSleep; //!< whether Aux PHY should be put into sleep mode while the Main PHY
                          //!< is operating on the same link as the Aux PHY
    EventId m_auxPhyToSleepEvent;     //!< the event scheduled to put an Aux PHY into sleep mode
    Ptr<WifiPhy> m_auxPhyToReconnect; //!< Aux PHY the ChannelAccessManager of the link on which
                                      //!< the main PHY is operating has to connect a listener to
                                      //!< when the main PHY is back operating on its previous link
};

} // namespace ns3

#endif /* DEFAULT_EMLSR_MANAGER_H */

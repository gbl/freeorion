#include "MultiIconValueIndicator.h"

#include <GG/Texture.h>

#include <chrono>

#include "../util/i18n.h"
#include "../util/Logger.h"
#include "../universe/PopCenter.h"
#include "../universe/UniverseObject.h"
#include "../universe/Enums.h"
#include "../universe/Fleet.h"
#include "../universe/Planet.h"
#include "../universe/Ship.h"
#include "../universe/ShipDesign.h"
#include "../client/human/HumanClientApp.h"
#include "../Empire/Empire.h"
#include "../Empire/ProductionQueue.h"
#include "ClientUI.h"
#include "CUIControls.h"

namespace {
    const int   EDGE_PAD(3);

    int         IconSpacing()
    { return ClientUI::Pts(); }
    GG::X       IconWidth()
    { return GG::X(IconSpacing()*2); }
    GG::Y       IconHeight()
    { return GG::Y(IconSpacing()*2); }
}

namespace OutposterData {
    int last_updated_turn;
    std::map<int, int> outposts_by_system;
    std::map<int, int> colonies_by_system;
    
    void add_to_map(std::map<int, int>& map, int key, int val) {
        if (map.count(key) == 0) {
            map[key] = val;
        } else {
            map[key] = map[key] + val;
        }
    }

    void add_design_to_map(const ShipDesign *design, int destination_id, int multiplier) {
        int outposters = 0, colonizers = 0;
        for (auto part_name : design->Parts()) {
            if (part_name.empty())
                continue;
            auto type = GetPartType(part_name);
            if (type->Class() != PC_COLONY)
                continue;
            if (type->Capacity() == 0) {
                outposters+=multiplier;
            } else {
                colonizers+=multiplier;
            }
        }
        if (outposters == 0 && colonizers == 0)
            return;
        std::cout << "design counting: " << design->Name() << " to " << destination_id << std::endl;
        std::cout << "  colonizers " << colonizers << ", outposters " << outposters << std::endl;
        add_to_map(outposts_by_system, destination_id, outposters);
        add_to_map(colonies_by_system, destination_id, colonizers);
    }

    void CountShipsInProduction(const Empire *empire) {
        const ProductionQueue& queue = empire->GetProductionQueue();
        for (auto element: queue) {
            ProductionQueue::ProductionItem item = element.item;
            if (item.build_type != BT_SHIP)
                continue;
            int destination_id = element.rally_point_id;
            if (destination_id == INVALID_OBJECT_ID) {
                destination_id = element.location; // production planet
                auto planet = GetPlanet(destination_id);
                destination_id = planet->SystemID();
            }
            const ShipDesign *design = GetShipDesign(item.design_id);
            std::cout << "building " << element.remaining * element.blocksize << " " << design->Name(false) << 
                    " ships at " << element.location << ", rally point " << element.rally_point_id << 
                    " gives destination " << destination_id << std::endl;
            add_design_to_map(design, destination_id, element.remaining * element.blocksize);
        }
    }
    
    void CountShipsInFlight(const Empire *empire) {
        for (const auto& entry : Objects().ExistingFleets()) {
            std::shared_ptr<const Fleet> fleet = std::dynamic_pointer_cast<const Fleet>(entry.second);
            if (fleet->Empty()) {
                continue;
            }
            if (fleet->Owner() != empire->EmpireID()) {
                continue;
            }
            for (int shipid: fleet->ShipIDs()) {
                auto ship = GetShip(shipid);
                const ShipDesign *design = GetShipDesign(ship->DesignID());
                // std::cout << "fleet with " << design->Name() << " moving to " << fleet->FinalDestinationID() << std::endl;
                add_design_to_map(design, fleet->FinalDestinationID(), 1);
            }
        }
    }
    
    void UpdateColonizersAndOutposters(const Empire *empire) {
        // we also need to update when the player sets the destination
        // or rally point of a ship (in building) which this logic
        // prevents. If things are fast enough just scrap this.
        /*
        if (last_updated_turn == CurrentTurn())
            return;
        last_updated_turn = CurrentTurn();
        */

        auto start = std::chrono::steady_clock::now();

        outposts_by_system.clear();
        colonies_by_system.clear();

        CountShipsInProduction(empire);
        CountShipsInFlight(empire);

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start
        ).count();
        std::cout << "Time for counting colonizers: " << elapsed << std::endl;
    }
}

MultiIconValueIndicator::MultiIconValueIndicator(GG::X w) {
    MultiIconValueIndicator(w, INVALID_OBJECT_ID);
}

MultiIconValueIndicator::MultiIconValueIndicator(GG::X w, int system_id) :
    MultiIconValueIndicator(w, system_id, {}, {})
{}

MultiIconValueIndicator::MultiIconValueIndicator(
    GG::X w, int object_id,
    const std::vector<std::pair<MeterType, MeterType>>& meter_types) :
    MultiIconValueIndicator(w, INVALID_OBJECT_ID, std::vector<int>{object_id}, meter_types)
{}

MultiIconValueIndicator::MultiIconValueIndicator(
    GG::X w, int system_id, int object_id,
    const std::vector<std::pair<MeterType, MeterType>>& meter_types) :
    MultiIconValueIndicator(w, system_id, std::vector<int>{object_id}, meter_types)
{}

MultiIconValueIndicator::MultiIconValueIndicator(
    GG::X w, int system_id, const std::vector<int>& object_ids,
    const std::vector<std::pair<MeterType, MeterType>>& meter_types) :
    GG::Wnd(GG::X0, GG::Y0, w, GG::Y1, GG::INTERACTIVE),
    m_icons(),
    m_system_id(system_id),
    m_meter_types(meter_types),
    m_object_ids(object_ids)
{
    std::cout << "creating MultiIconValueIndicator, system " << m_system_id << " has " << m_object_ids.size() << " items " << std::endl;
}

void MultiIconValueIndicator::CompleteConstruction() {
    GG::Wnd::CompleteConstruction();

    SetName("MultiIconValueIndicator");

    GG::X x(EDGE_PAD);
    for (const std::pair<MeterType, MeterType>& meter_type : m_meter_types) {
        const MeterType PRIMARY_METER_TYPE = meter_type.first;
        // get icon texture.
        std::shared_ptr<GG::Texture> texture = ClientUI::MeterIcon(PRIMARY_METER_TYPE);

        // special case for population meter for an indicator showing only a
        // single popcenter: icon is species icon, rather than generic pop icon
        if (PRIMARY_METER_TYPE == METER_POPULATION && m_object_ids.size() == 1) {
            if (auto pc = GetPopCenter(*m_object_ids.begin()))
                texture = ClientUI::SpeciesIcon(pc->SpeciesName());
        }

        m_icons.push_back(GG::Wnd::Create<StatisticIcon>(texture, 0.0, 3, false, IconWidth(), IconHeight()));
        GG::Pt icon_ul(x, GG::Y(EDGE_PAD));
        GG::Pt icon_lr = icon_ul + GG::Pt(IconWidth(), IconHeight() + ClientUI::Pts()*3/2);
        m_icons.back()->SizeMove(icon_ul, icon_lr);
        m_icons.back()->InstallEventFilter(shared_from_this());
        AttachChild(m_icons.back());
        x += IconWidth() + IconSpacing();
    }

    int empire_id = HumanClientApp::GetApp()->EmpireID();
    Empire* empire = GetEmpire(empire_id);
    if (m_system_id != INVALID_OBJECT_ID && empire) {
        OutposterData::UpdateColonizersAndOutposters(empire);
        x = Width() - EDGE_PAD - IconWidth();

        if (OutposterData::colonies_by_system[m_system_id]) {
            std::cout << "Colonizers: " << OutposterData::colonies_by_system[m_system_id] << " to system " << m_system_id << std::endl;
            auto texture = ClientUI::GetTexture(ClientUI::ArtDir() / "icons" / "fleet" / "med-head-colony.png");
            m_icons.push_back(GG::Wnd::Create<StatisticIcon>(texture, (double) OutposterData::colonies_by_system[m_system_id], 0, false, IconWidth(), IconHeight()));
            GG::Pt icon_ul(x, GG::Y(EDGE_PAD));
            GG::Pt icon_lr = icon_ul + GG::Pt(IconWidth(), IconHeight() + ClientUI::Pts()*3/2);
            m_icons.back()->SizeMove(icon_ul, icon_lr);
            AttachChild(m_icons.back());
            x -= IconWidth() + IconSpacing();
        }

        if (OutposterData::outposts_by_system[m_system_id]) {
            std::cout << "Outposters: " << OutposterData::outposts_by_system[m_system_id] << " to system " << m_system_id << std::endl;
            auto texture = ClientUI::GetTexture(ClientUI::ArtDir() / "icons" / "fleet" / "med-head-outpost.png");
            m_icons.push_back(GG::Wnd::Create<StatisticIcon>(texture, (double) OutposterData::outposts_by_system[m_system_id], 0, false, IconWidth(), IconHeight()));
            GG::Pt icon_ul(x, GG::Y(EDGE_PAD));
            GG::Pt icon_lr = icon_ul + GG::Pt(IconWidth(), IconHeight() + ClientUI::Pts()*3/2);
            m_icons.back()->SizeMove(icon_ul, icon_lr);
            AttachChild(m_icons.back());
            x -= IconWidth() + IconSpacing();
        }
    }

    if (!m_icons.empty())
        Resize(GG::Pt(Width(), EDGE_PAD + IconHeight() + ClientUI::Pts()*3/2));
    Update();
}

/** Are there any planets in this system? NB this is not about the number of meters. */
bool MultiIconValueIndicator::Empty() const
{
    std::cout << "MultiIconValueIndicator::Empty for system " << m_system_id << " has " << m_object_ids.size() << " objects " << std::endl;
    return m_object_ids.empty();
}

void MultiIconValueIndicator::Render() {
    GG::Pt ul = UpperLeft();
    GG::Pt lr = LowerRight();

    // outline of whole control
    GG::FlatRectangle(ul, lr, ClientUI::WndColor(), ClientUI::WndOuterBorderColor(), 1);
}

void MultiIconValueIndicator::MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys)
{ ForwardEventToParent(); }

void MultiIconValueIndicator::Update() {
    /** cant use this anymore as we may have colonizer icons
    if (m_icons.size() != m_meter_types.size()) {
        ErrorLogger() << "MultiIconValueIndicator::Update has inconsitent numbers of icons and meter types";
        return;
    }*/

    for (std::size_t i = 0; i < m_meter_types.size(); ++i) {
        assert(m_icons[i]);
        double total = 0.0;
        for (int object_id : m_object_ids) {
            auto obj = GetUniverseObject(object_id);
            if (!obj) {
                ErrorLogger() << "MultiIconValueIndicator::Update couldn't get object with id " << object_id;
                continue;
            }
            //DebugLogger() << "MultiIconValueIndicator::Update object:";
            //DebugLogger() << obj->Dump();
            auto type = m_meter_types[i].first;
            double value = obj->InitialMeterValue(type);
            // Supply is a special case: the only thing that matters is the highest value.
            if (type == METER_SUPPLY)
                total = std::max(total, value);
            else
                total += value;
        }
        m_icons[i]->SetValue(total);
    }
}

void MultiIconValueIndicator::SetToolTip(MeterType meter_type, const std::shared_ptr<GG::BrowseInfoWnd>& browse_wnd) {
    for (unsigned int i = 0; i < m_meter_types.size(); ++i)
        if (m_meter_types.at(i).first == meter_type)
            m_icons.at(i)->SetBrowseInfoWnd(browse_wnd);
}

void MultiIconValueIndicator::ClearToolTip(MeterType meter_type) {
    for (unsigned int i = 0; i < m_meter_types.size(); ++i)
        if (m_meter_types.at(i).first == meter_type)
            m_icons.at(i)->ClearBrowseInfoWnd();
}

bool MultiIconValueIndicator::EventFilter(GG::Wnd* w, const GG::WndEvent& event) {
    if (event.Type() != GG::WndEvent::RClick)
        return false;
    const GG::Pt& pt = event.Point();

    MeterType meter_type = INVALID_METER_TYPE;
    for (unsigned int i = 0; i < m_meter_types.size(); ++i) {
        try {
            if (m_icons.at(i).get() == w) {
                meter_type = m_meter_types.at(i).first;
                break;
            }
        } catch(std::out_of_range &e) {
            ErrorLogger() << e.what();
            return false;
        }
    }
    if (meter_type == INVALID_METER_TYPE)
        return false;

    std::string meter_string = boost::lexical_cast<std::string>(meter_type);
    std::string meter_title;
    if (UserStringExists(meter_string))
        meter_title = UserString(meter_string);

    std::string species_name;

    bool retval = false;
    auto zoom_species_action = [&retval, &species_name]() { retval = ClientUI::GetClientUI()->ZoomToSpecies(species_name); };
    auto zoom_article_action = [&retval, &meter_string]() { retval = ClientUI::GetClientUI()->ZoomToMeterTypeArticle(meter_string);};

    auto popup = GG::Wnd::Create<CUIPopupMenu>(pt.x, pt.y);

    if (m_object_ids.size() > 0) {
        auto pc = GetPopCenter(*m_object_ids.begin());
        if (meter_type == METER_POPULATION && pc && m_object_ids.size() == 1) {
            species_name = pc->SpeciesName();
            if (!species_name.empty()) {
                std::string species_label = boost::io::str(FlexibleFormat(UserString("ENC_LOOKUP")) % UserString(species_name));
                popup->AddMenuItem(GG::MenuItem(species_label, false, false, zoom_species_action));
            }
        }
    }

    if (!meter_title.empty()) {
        std::string popup_label = boost::io::str(FlexibleFormat(UserString("ENC_LOOKUP")) %
                                                                meter_title);
        popup->AddMenuItem(GG::MenuItem(popup_label, false, false, zoom_article_action));
    }
    popup->Run();

    return retval;
}

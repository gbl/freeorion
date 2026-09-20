#include "SpecialsListWnd.h"

#include "CUIControls.h"
#include "../client/human/HumanClientApp.h"
#include "../Empire/Empire.h"
#include "../Empire/EmpireManager.h"
#include "../network/Message.h"
#include "../network/ClientNetworking.h"
#include "../util/i18n.h"
#include "../util/Logger.h"
#include "../util/OptionsDB.h"
#include "../universe/Ship.h"
#include "../universe/Special.h"
#include "../universe/Planet.h"
#include "../universe/System.h"
#include "../universe/Enums.h"
#include "../util/VarText.h"

#include <GG/DrawUtil.h>

#include <algorithm>

namespace {
    const int           DATA_PANEL_BORDER = 1;

    /** A header row - Lithic, Organic, Robotic, Research */
    class SpecialsListHeader : public GG::Control {
   
    public:
        SpecialsListHeader(GG::X left, GG::Y top, GG::X w, GG::Y h, bool level1, std::string text): 
            Control(left, top, w, h, GG::NO_WND_FLAGS),
            m_level1(level1),
            m_header_text(text)
        {}

        void CompleteConstruction() override {
            GG::Control::CompleteConstruction();
            SetChildClippingMode(ClipToClient);     // line wrap here ?? 
            m_header = GG::Wnd::Create<CUILabel>(UserString(m_header_text), GG::FORMAT_LEFT);   // bold?
            if (m_level1) {
                m_header->SetFont(ClientUI::GetBoldFont());
            }
            else {
                m_header->SetFont(ClientUI::GetBoldFont());
		m_icon = GG::Wnd::Create<StatisticIcon>(ClientUI::SpecialIcon(
			m_header_text), GG::X(20), GG::Y(20));
	    }
            GG::Pt size = m_header->MinUsableSize(Width());
            size.x = Width();
            // std::cout << "Resizing " << m_header_text << " to " << size << std::endl;
            Resize(size);
	    if (m_icon) {
	        AttachChild(m_icon);
		m_header->SizeMove(GG::Pt(GG::X(25), GG::Y0), GG::Pt(Width()-25, Height()));
	    } else {
                m_header->Resize(size);
	    }
            AttachChild(m_header);
            DoLayout();
        }

        /** Excludes border from the client area. */
        GG::Pt ClientUpperLeft() const override
        { return UpperLeft() + GG::Pt(GG::X(DATA_PANEL_BORDER), GG::Y(DATA_PANEL_BORDER)); }

        /** Excludes border from the client area. */
        GG::Pt ClientLowerRight() const override
        { return LowerRight() - GG::Pt(GG::X(DATA_PANEL_BORDER), GG::Y(DATA_PANEL_BORDER)); }


        void Render() {
            // std::cout << "level1 is " << m_level1 << ", rendering for " << m_header_text << " in rect "
            //    << ClientUpperLeft() <<  " to " << ClientLowerRight() << std::endl;
            if (m_level1) {
                GG::FlatRectangle(ClientUpperLeft(), ClientLowerRight(), ClientUI::WndOuterBorderColor(),
                    GG::CLR_ZERO, 0);
            }
        }

        private:
            bool m_level1;
            std::string m_header_text;
	    std::shared_ptr<StatisticIcon> m_icon;
            std::shared_ptr<GG::Label>               m_header;
            
        void DoLayout() {
        }
    };


	// copied from ColorByOwner, expanded

	class ColorPlanet: public LinkDecorator {
	public:
		ColorPlanet(std::string focustype) :
			m_focustype(focustype)
		{}

		std::string Decorate(const std::string& object_id_str, const std::string& content) const {
			GG::Clr color = GG::CLR_WHITE;
			const Empire* empire = nullptr;
			// get object indicated by object_id, and then get object's owner, if any
			int object_id = CastStringToInt(object_id_str);
			auto object = Objects().Object(object_id);
			if (object && !object->Unowned())
				empire = GetEmpire(object->Owner());
			if (empire)
				color = empire->Color();
			auto planet = GetPlanet(object_id);
			std::string sfocus = "", efocus = "";
			if (planet && planet->Focus() == m_focustype) {
			    sfocus="<u>";
			    efocus="</u>";
			}
			return sfocus + GG::RgbaTag(color) + content + "</rgba>" + efocus;
		}
    private:
		std::string m_focustype;
	};

	void HandleLinkClick(const std::string& link_type, const std::string& data) {
		if (link_type == VarText::PLANET_ID_TAG) {
			ClientUI::GetClientUI()->ZoomToPlanet(atoi(data.c_str()));
		}
	}

    ////////////////////////////////////////////////
    // SpecialsListPanel
    ////////////////////////////////////////////////
    class SpecialsListPanel : public GG::Control {
    public:
        SpecialsListPanel(GG::X w, GG::Y h, std::string text,
                std::string focustype,
                std::string spec1, std::string spec2, std::string spec3) :
            Control(GG::X0, GG::Y0, w, h, GG::NO_WND_FLAGS),
            m_text(text),
            m_focustype(focustype),
            m_spec1(spec1),
            m_spec2(spec2),
            m_spec3(spec3),
            m_ptext(nullptr),
            m_p1(nullptr), m_p2(nullptr), m_p3(nullptr),
            m_v1(nullptr), m_v2(nullptr), m_v3(nullptr)
        {
            SetName("SpecialsListPanel");
            SetChildClippingMode(ClipToClient);
        }

        void ConstructEntry(int& y, std::shared_ptr<SpecialsListHeader> &head,
            std::shared_ptr<LinkText> &body, std::string &special) {
            if (special.empty()) return;
            y+=25;
            // std::cout << "creating line for " << special << " at y=" << y << std::endl;
            head = GG::Wnd::Create<SpecialsListHeader>(GG::X0, GG::Y(y), Width(), GG::Y(25), false, special);
            AttachChild(head);
            y+=25;
            body = GG::Wnd::Create<LinkText>(GG::X0, GG::Y(y), Width(), special,
                ClientUI::GetFont(),
                GG::FORMAT_LEFT | GG::FORMAT_VCENTER | GG::FORMAT_WORDBREAK, ClientUI::TextColor());
            body -> SetDecorator(VarText::PLANET_ID_TAG, new ColorPlanet(m_focustype));
            body -> LinkClickedSignal.connect(&HandleLinkClick);
            AttachChild(body);
        }

        void CompleteConstruction() override {
            int y = 0;
            GG::Control::CompleteConstruction();
            SetChildClippingMode(ClipToClient);
            m_ptext = GG::Wnd::Create<SpecialsListHeader>(GG::X0, GG::Y(y), Width(), GG::Y(25), true, m_text);
            AttachChild(m_ptext);
            ConstructEntry(y, m_p1, m_v1, m_spec1);
            ConstructEntry(y, m_p2, m_v2, m_spec2);
            ConstructEntry(y, m_p3, m_v3, m_spec3);
            Update();
        }

        void    Update() {
            if (m_v1) { FillPlanets(m_v1, m_spec1); }
            if (m_v2) { FillPlanets(m_v2, m_spec2); }
            if (m_v3) { FillPlanets(m_v3, m_spec3); }
            DoLayout();
        }

        void    FillPlanets(std::shared_ptr<LinkText> widget, std::string& spectype) {
            // std::cout << "Filling Planets with special " << spectype << " and focus " << m_focustype << std::endl;
			int client_empire_id = HumanClientApp::GetApp()->EmpireID();
			const std::set<int> objectids = GetUniverse().EmpireVisibleObjectIDs(client_empire_id);
			std::string planetlist = "";
            VarText vartext(planetlist, false);
            int foundplanets = 0;
            PlanetSize last_planet_size = INVALID_PLANET_SIZE;

            std::vector<int> planets_by_size;
            std::copy_if(objectids.begin(), objectids.end(),
                std::inserter(planets_by_size, planets_by_size.end()),
                          [](int x) { return GetPlanet(x); });

            // Order planets by size, but asteroid belts first
            std::sort(planets_by_size.begin(), planets_by_size.end(), [] (int a, int b) {
                auto planeta = GetPlanet(a); if (!planeta) return true;    // should not happen
                auto planetb = GetPlanet(b); if (!planetb) return false;    // should not happen
                if (planeta->Size() == planetb->Size()) {
                    return planeta->Name() < planetb->Name();
                }
                if (planeta->Size() == SZ_ASTEROIDS) {
                    return true;
                }
                if (planetb->Size() == SZ_ASTEROIDS) {
                    return false;
                }
                return planeta->Size() < planetb->Size();
            });
			for (const auto planetid: planets_by_size) {
				auto planet = GetPlanet(planetid);
				if (!planet) { continue; }  // should not happen
				// std::cout << "testing " << planet->Name() << std::endl;
				for (const auto entry: planet->Specials()) {
                    const Special *special = GetSpecial(entry.first);
					// std::cout << "  object id " << planetid << " (" << planet->Name() << ") has special " << special->Name() << std::endl;
					if (special->Name() == spectype) {
						// std::cout << "    planet has focus " << planet->Focus() << std::endl;
                        foundplanets++;
                        std::string varname = "p" + std::to_string(foundplanets);
                        vartext.AddVariable(varname, std::to_string(planetid));

						if (!planetlist.empty()) {
							planetlist += ", ";
						}
                        if (last_planet_size != planet->Size()) {
                            char letter = UserString(boost::lexical_cast<std::string>(planet->Size())).at(0);
                            planetlist += "<rgba 255 0 0 255>";
                            planetlist += letter;
                            planetlist += ":</rgba> ";
                            last_planet_size = planet->Size();
                        }
						planetlist += "%planet:"+varname+"%";
					}
				}
			}
            vartext.SetTemplateString(planetlist+"  ", false);	// need the blanks as we dont get a link at the end
			widget->SetText(vartext.GetText());
        }

        int DoSingleLayout(std::shared_ptr<SpecialsListHeader> control, int y) {
            if (!control) { return 0; }
            GG::Pt lt (GG::X0, GG::Y(y));
            GG::Pt size(Width(), GG::Y(25));
            GG::Pt rb(lt + size);
            // std::cout << "doSingleLayout moving control to " << lt << " / " << rb << std::endl;
            control->SizeMove(lt, rb);
            return 25;
        }

        int DoSingleLayout(std::shared_ptr<LinkText> control, int y) {
            if (!control) { return 0; }
            GG::Pt lt (GG::X0, GG::Y(y));
            GG::Pt size = control->MinUsableSize(Width());
            size.x = Width();
	    size.y += 3;	// make sure we can show the underline
            GG::Pt rb(lt + size);
            // std::cout << "doSingleLayout moving text to " << lt << " / " << rb << std::endl;
            control->SizeMove(lt, rb);
            return Value(size.y);
        }

        int DoLayout() {
            int y = 0;
            y += DoSingleLayout(m_ptext, y);
            y += DoSingleLayout(m_p1, y);
            y += DoSingleLayout(m_v1, y);
            y += DoSingleLayout(m_p2, y);
            y += DoSingleLayout(m_v2, y);
            y += DoSingleLayout(m_p3, y);
            y += DoSingleLayout(m_v3, y);

            return y;
        }

        /** This function overridden because otherwise, rows don't expand
          * larger than their initial size when resizing the list. */
        void SizeMove(const GG::Pt& ul, const GG::Pt& lr) override {
            const GG::Pt old_size = Size();
            GG::Control::SizeMove(ul, lr);
            //std::cout << "SpecialsRow::SizeMove size: (" << Value(Width()) << ", " << Value(Height()) << ")" << std::endl;
            if (old_size != Size()) {
                int neededHeight = DoLayout();
                GG::Pt newSize = GG::Pt(Width(), GG::Y(neededHeight));
                // why does this work in SitRepPanel:324 but not here?
                // std::cout << "resizing panel to " << newSize << std::endl;
                GG::Control::SizeMove(ul, ul+newSize);
            }
        }

    private:
        std::string m_text, m_focustype, m_spec1, m_spec2, m_spec3;
        std::shared_ptr<SpecialsListHeader>    m_ptext, m_p1, m_p2, m_p3;
        std::shared_ptr<LinkText> m_v1, m_v2, m_v3;
    };

    ////////////////////////////////////////////////
    // SpecialsListRow
    ////////////////////////////////////////////////
    class SpecialsListRow : public GG::ListBox::Row {
    public:
        SpecialsListRow(GG::X w, GG::Y h, std::string text,
        std::string focustype,
        std::string spec1, std::string spec2, std::string spec3) :
            GG::ListBox::Row(w, h, "", GG::ALIGN_NONE, 0),
            m_text(text),
            m_focustype(focustype),
            m_spec1(spec1),
            m_spec2(spec2),
            m_spec3(spec3)
        {
            SetName("SpecialsListRow");
            SetChildClippingMode(ClipToClient);
        }

        void CompleteConstruction() override {
            GG::ListBox::Row::CompleteConstruction();
            m_panel = GG::Wnd::Create<SpecialsListPanel>(Width(), Height(),
                    m_text, m_focustype, m_spec1, m_spec2, m_spec3);
            push_back(m_panel);
        }

        void    Update() {
            if (m_panel)
                m_panel->Update();
        }

        /** This function overridden because otherwise, rows don't expand
          * larger than their initial size when resizing the list. */
        void SizeMove(const GG::Pt& ul, const GG::Pt& lr) override {
            const GG::Pt old_size = Size();
            GG::ListBox::Row::SizeMove(ul, lr);
            // std::cout << "SpecialsRow::SizeMove size: (" << Value(Width()) << ", " << Value(Height()) << ")" << std::endl;
            if (!empty() && old_size != Size() && m_panel) {
		// std::cout << "panel size : " << m_panel->Size() << std::endl;
		GG::Pt size = m_panel->Size();
		size.x = Width();
		Resize(size);
	    }
        }
    private:
        std::shared_ptr<SpecialsListPanel>    m_panel;
        std::string m_text, m_focustype, m_spec1, m_spec2, m_spec3;
    };
}

////////////////////////////////////////////////
// SpecialsListBox
////////////////////////////////////////////////
class SpecialsListBox : public CUIListBox {
public:
    SpecialsListBox(void) :
        CUIListBox()
    {
        // preinitialize listbox/row column widths, because what
        // ListBox::Insert does on default is not suitable for this case
        SetNumCols(1);
        SetColWidth(0, GG::X0);
        LockColWidths();
    }

    void SizeMove(const GG::Pt& ul, const GG::Pt& lr) override {
        const GG::Pt old_size = Size();
        CUIListBox::SizeMove(ul, lr);
        //std::cout << "SpecialsListBox::SizeMove size: (" << Value(Width()) << ", " << Value(Height()) << ")" << std::endl;
	/* GBL remove this, rows know their own size */
        if (old_size != Size()) {
            const GG::Pt row_size = ListRowSize();
            //std::cout << "SpecialsListBox::SizeMove list row size: (" << Value(row_size.x) << ", " << Value(row_size.y) << ")" << std::endl;
            for (auto& row : *this)
                row->Resize(row_size);
        }
    }

    GG::Pt          ListRowSize() const
    { return GG::Pt(Width() - ClientUI::ScrollWidth() - 5, ListRowHeight()); }

    static GG::Y    ListRowHeight()
    { return GG::Y(ClientUI::Pts() * 3/2 * 7); }
};


/////////////////////
//  SpecialsListWnd  //
/////////////////////
SpecialsListWnd::SpecialsListWnd(const std::string& config_name) :
    CUIWnd(UserString("SPECIALS_LIST_PANEL_TITLE"),
           GG::INTERACTIVE | GG::DRAGABLE | GG::ONTOP | GG::RESIZABLE | CLOSABLE | PINABLE,
           config_name),
    m_specials_list(nullptr)
{}

void SpecialsListWnd::CompleteConstruction() {
    CUIWnd::CompleteConstruction();

    m_specials_list = GG::Wnd::Create<SpecialsListBox>();
    m_specials_list->SetHiliteColor(GG::CLR_ZERO);
    m_specials_list->SetStyle(GG::LIST_NOSORT);
    AttachChild(m_specials_list);

    DoLayout();

    Refresh();
}

void SpecialsListWnd::HandlePlayerStatusUpdate(Message::PlayerStatus player_status, int about_player_id) {
}

void SpecialsListWnd::Update() {
}

void SpecialsListWnd::Refresh() {
    m_specials_list->Clear();
    
    // move this to a config file? There isn't one that's suitable rn.

    const GG::Pt row_size = m_specials_list->ListRowSize();
    auto organic_row = GG::Wnd::Create<SpecialsListRow>(row_size.x, row_size.y, "ORGANIC", "FOCUS_GROWTH",
        "SPICE_SPECIAL", "FRUIT_SPECIAL", "PROBIOTIC_SPECIAL");
    auto lithic_row = GG::Wnd::Create<SpecialsListRow>(row_size.x, row_size.y, "LITHIC", "FOCUS_GROWTH",
        "CRYSTALS_SPECIAL", "MINERALS_SPECIAL", "ELERIUM_SPECIAL");
    auto robotic_row = GG::Wnd::Create<SpecialsListRow>(row_size.x, row_size.y, "ROBOTIC", "FOCUS_GROWTH",
        "MONOPOLE_SPECIAL", "SUPERCONDUCTOR_SPECIAL", "POSITRONIUM_SPECIAL");
    auto compmoon_row = GG::Wnd::Create<SpecialsListRow>(row_size.x, row_size.y, "COMPUTRONIUM_SPECIAL",
                                                                                    "FOCUS_RESEARCH",
        "COMPUTRONIUM_SPECIAL", "", "");

    m_specials_list->Insert(organic_row);
    m_specials_list->Insert(lithic_row);
    m_specials_list->Insert(robotic_row);
    m_specials_list->Insert(compmoon_row);
    // TODO GBL adjust heights like in later SizeMoves
}

void SpecialsListWnd::Clear() {
    m_specials_list->Clear();
}

void SpecialsListWnd::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    const GG::Pt old_size = Size();
    CUIWnd::SizeMove(ul, lr);
    if (old_size != Size())
        DoLayout();
}

void SpecialsListWnd::DoLayout()
{
    if (m_specials_list)
        m_specials_list->SizeMove(GG::Pt(), GG::Pt(ClientWidth(), ClientHeight() - GG::Y(INNER_BORDER_ANGLE_OFFSET)));
}

void SpecialsListWnd::CloseClicked()
{ ClosingSignal(); }


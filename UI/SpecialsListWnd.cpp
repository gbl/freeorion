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
#include "../universe/Planet.h"
#include "../universe/System.h"
#include "../universe/Enums.h"

#include <GG/DrawUtil.h>

#include <algorithm>

namespace {
    const int           DATA_PANEL_BORDER = 1;

    /** A header row - Lithic, Organic, Robotic, Research */
    class SpecialsListHeader : public GG::Control {
   
    public:
        SpecialsListHeader(GG::X w, GG::Y h, std::string text): 
            Control(GG::X0, GG::Y0, w, h, GG::NO_WND_FLAGS),
            m_header_text(text)
        {}

        void CompleteConstruction() override {
            GG::Control::CompleteConstruction();
            SetChildClippingMode(ClipToClient);     // line wrap here ?? 
            m_header = GG::Wnd::Create<CUILabel>(m_header_text, GG::FORMAT_LEFT);   // bold?
            m_header->SetFont(ClientUI::GetBoldFont());
            GG::Pt size = m_header->MinUsableSize(Width());
            m_header->Resize(size);
            Resize(size);
            AttachChild(m_header);
            DoLayout();
            Update();
        }

        /** Excludes border from the client area. */
        GG::Pt ClientUpperLeft() const override
        { return UpperLeft() + GG::Pt(GG::X(DATA_PANEL_BORDER), GG::Y(DATA_PANEL_BORDER)); }

        /** Excludes border from the client area. */
        GG::Pt ClientLowerRight() const override
        { return LowerRight() - GG::Pt(GG::X(DATA_PANEL_BORDER), GG::Y(DATA_PANEL_BORDER)); }


        void Render() {
            GG::FlatRectangle(ClientUpperLeft(), ClientLowerRight(), ClientUI::WndOuterBorderColor(),
                GG::CLR_ZERO, 0);
            /*
            GG::Control::Render();
            */
        }

        void Update() {
        }

        private:
            std::string m_header_text;
            std::shared_ptr<GG::Label>               m_header;
            
        void DoLayout() {
        }
    };

    ////////////////////////////////////////////////
    // PlayerRow
    ////////////////////////////////////////////////
    class SpecialsListRow : public GG::ListBox::Row {
    public:
        SpecialsListRow(GG::X w, GG::Y h, std::string text) :
            GG::ListBox::Row(w, h, "", GG::ALIGN_NONE, 0),
            m_text(text),
            m_panel(nullptr)
        {
            SetName("SpecialsListRow");
            SetChildClippingMode(ClipToClient);
        }

        void CompleteConstruction() override {

            GG::ListBox::Row::CompleteConstruction();
            m_panel = GG::Wnd::Create<SpecialsListHeader>(Width(), Height(), m_text);
            push_back(m_panel);
        }

        std::string     RowName() const {
            return m_text;
        }

        void    Update() {
            if (m_panel)
                m_panel->Update();
        }

        void    SetStatus(Message::PlayerStatus player_status) {
        }

        /** This function overridden because otherwise, rows don't expand
          * larger than their initial size when resizing the list. */
        void SizeMove(const GG::Pt& ul, const GG::Pt& lr) override {
            const GG::Pt old_size = Size();
            GG::ListBox::Row::SizeMove(ul, lr);
            //std::cout << "SpecialsRow::SizeMove size: (" << Value(Width()) << ", " << Value(Height()) << ")" << std::endl;
            if (!empty() && old_size != Size() && m_panel)
                m_panel->Resize(Size());
        }

    private:
        std::string                 m_text;
        std::shared_ptr<SpecialsListHeader>    m_panel;
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
    { return GG::Y(ClientUI::Pts() * 3/2); }  // to change dependent on text height?
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
    
    const GG::Pt row_size = m_specials_list->ListRowSize();
    auto organic_row = GG::Wnd::Create<SpecialsListRow>(row_size.x, row_size.y, "Organic");
    auto lithic_row = GG::Wnd::Create<SpecialsListRow>(row_size.x, row_size.y, "Lithic");
    auto robotic_row = GG::Wnd::Create<SpecialsListRow>(row_size.x, row_size.y, "Robotic");
    auto compmoon_row = GG::Wnd::Create<SpecialsListRow>(row_size.x, row_size.y, "Computronium Moon");

    m_specials_list->Insert(organic_row);
    m_specials_list->Insert(lithic_row);
    m_specials_list->Insert(robotic_row);
    m_specials_list->Insert(compmoon_row);

    // ?? needed? player_row->Resize(row_size);
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
    std::cout << "Specials List Do Layout" << GG::Pt() << " " << ClientWidth() << " " << ClientHeight() << std::endl;
    if (m_specials_list)
        m_specials_list->SizeMove(GG::Pt(), GG::Pt(ClientWidth(), ClientHeight() - GG::Y(INNER_BORDER_ANGLE_OFFSET)));
}

void SpecialsListWnd::CloseClicked()
{ ClosingSignal(); }


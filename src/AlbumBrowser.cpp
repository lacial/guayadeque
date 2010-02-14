// -------------------------------------------------------------------------------- //
//	Copyright (C) 2008-2009 J.Rios
//	anonbeat@gmail.com
//
//    This Program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2, or (at your option)
//    any later version.
//
//    This Program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; see the file LICENSE.  If not, write to
//    the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//    http://www.gnu.org/copyleft/gpl.html
//
// -------------------------------------------------------------------------------- //
#include "AlbumBrowser.h"

#include "Commands.h"
#include "Config.h"
#include "CoverEdit.h"
#include "Images.h"
#include "LabelEditor.h"
#include "MainApp.h"
#include "OnlineLinks.h"
#include "TagInfo.h"
#include "TrackEdit.h"
#include "Utils.h"

#include <wx/arrimpl.cpp>
#include "wx/clipbrd.h"
#include <wx/menu.h>

WX_DEFINE_OBJARRAY( guAlbumBrowserItemArray );

#define guALBUMBROWSER_REFRESH_DELAY    60

void AddAlbumCommands( wxMenu * Menu, int SelCount );

// -------------------------------------------------------------------------------- //
class guUpdateAlbumDetails : public wxThread
{
  protected :
    guAlbumBrowser *        m_AlbumBrowser;
    guUpdateAlbumDetails ** m_SelfPtrStore;

  public :
    guUpdateAlbumDetails( guAlbumBrowser * albumbrowser, guUpdateAlbumDetails ** selfptr );
    ~guUpdateAlbumDetails();

    virtual ExitCode Entry();

};

// -------------------------------------------------------------------------------- //
guUpdateAlbumDetails::guUpdateAlbumDetails( guAlbumBrowser * albumbrowser, guUpdateAlbumDetails ** selfptr )
{
    m_AlbumBrowser = albumbrowser;
    m_SelfPtrStore = selfptr;

    if( Create() == wxTHREAD_NO_ERROR )
    {
        SetPriority( WXTHREAD_DEFAULT_PRIORITY );
        Run();
    }
}

// -------------------------------------------------------------------------------- //
guUpdateAlbumDetails::~guUpdateAlbumDetails()
{
    if( !TestDestroy() )
        * m_SelfPtrStore = NULL;
}

// -------------------------------------------------------------------------------- //
guUpdateAlbumDetails::ExitCode guUpdateAlbumDetails::Entry()
{
    if( !TestDestroy() )
    {
        //guLogMessage( wxT( "Searching details..." ) );
        guDbLibrary * Db = m_AlbumBrowser->m_Db;
        int Index;
        int Count = m_AlbumBrowser->m_AlbumItems.Count();
        if( Count )
        {
            for( Index = 0; Index < Count; Index++ )
            {
                if( TestDestroy() )
                    break;

                if( m_AlbumBrowser->m_AlbumItems[ Index ].m_AlbumId != ( unsigned int ) wxNOT_FOUND )
                {
                    m_AlbumBrowser->m_AlbumItems[ Index ].m_Year = Db->GetAlbumYear( m_AlbumBrowser->m_AlbumItems[ Index ].m_AlbumId );
                    m_AlbumBrowser->m_AlbumItems[ Index ].m_TrackCount = Db->GetAlbumTrackCount( m_AlbumBrowser->m_AlbumItems[ Index ].m_AlbumId );

                    if( TestDestroy() )
                        break;

                    wxCommandEvent event( wxEVT_COMMAND_MENU_SELECTED, ID_ALBUMBROWSER_UPDATEDETAILS );
                    event.SetInt( Index );
                    wxPostEvent( m_AlbumBrowser, event );
                    //guLogMessage( wxT( "Sent Details %i %i for %i" ), m_AlbumBrowser->m_AlbumItems[ Index ].m_Year, m_AlbumBrowser->m_AlbumItems[ Index ].m_TrackCount, Index );
                }
            }
        }
    }
    return 0;
}


// -------------------------------------------------------------------------------- //
// guAlbumBrowserItemPanel
// -------------------------------------------------------------------------------- //
guAlbumBrowserItemPanel::guAlbumBrowserItemPanel( wxWindow * parent, const int index,
    guAlbumBrowserItem * albumitem ) : wxPanel( parent, wxID_ANY )
{
    m_AlbumBrowserItem = albumitem;
    m_AlbumBrowser = ( guAlbumBrowser * ) parent;

    // GUI
	m_MainSizer = new wxBoxSizer( wxVERTICAL );

	m_Bitmap = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 100, 100 ), 0 );
	m_MainSizer->Add( m_Bitmap, 0, wxALIGN_CENTER_HORIZONTAL|wxTOP|wxRIGHT|wxLEFT, 5 );

	m_ArtistLabel = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
	m_ArtistLabel->Wrap( 120 );
	m_MainSizer->Add( m_ArtistLabel, 0, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT, 5 );

	m_AlbumLabel = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
	m_AlbumLabel->Wrap( 120 );
	m_MainSizer->Add( m_AlbumLabel, 0, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT, 5 );

	m_TracksLabel = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
	m_TracksLabel->Wrap( 120 );
	m_MainSizer->Add( m_TracksLabel, 0, wxALIGN_CENTER_HORIZONTAL|wxBOTTOM|wxRIGHT|wxLEFT, 5 );

	SetSizer( m_MainSizer );
	Layout();
	m_MainSizer->Fit( this );

    Connect( wxEVT_CONTEXT_MENU, wxContextMenuEventHandler( guAlbumBrowserItemPanel::OnContextMenu ), NULL, this );

    Connect( ID_ALBUM_COMMANDS, ID_ALBUM_COMMANDS + 99, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnCommandClicked ) );
    Connect( ID_LASTFM_SEARCH_LINK, ID_LASTFM_SEARCH_LINK + 999, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnSearchLinkClicked ) );

    Connect( ID_ALBUMBROWSER_PLAY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnPlayClicked ), NULL, this );
    Connect( ID_ALBUMBROWSER_ENQUEUE, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnEnqueueClicked ), NULL, this );
    Connect( ID_ALBUMBROWSER_EDITLABELS, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnAlbumEditLabelsClicked ), NULL, this );
    Connect( ID_ALBUMBROWSER_EDITTRACKS, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnAlbumEditTracksClicked ), NULL, this );
    Connect( ID_ALBUMBROWSER_COPYTOCLIPBOARD, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnCopyToClipboard ), NULL, this );
    Connect( ID_ALBUM_SELECTNAME, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnAlbumSelectName ), NULL, this );
    Connect( ID_ARTIST_SELECTNAME, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnArtistSelectName ), NULL, this );

    Connect( ID_ALBUMBROWSER_SEARCHCOVER, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnAlbumDownloadCoverClicked ), NULL, this );
    Connect( ID_ALBUMBROWSER_SELECTCOVER, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnAlbumSelectCoverClicked ), NULL, this );
    Connect( ID_ALBUMBROWSER_DELETECOVER, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnAlbumDeleteCoverClicked ), NULL, this );
    Connect( ID_ALBUMBROWSER_COPYTO, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnAlbumCopyToClicked ), NULL, this );
}

// -------------------------------------------------------------------------------- //
guAlbumBrowserItemPanel::~guAlbumBrowserItemPanel()
{
    Disconnect( wxEVT_CONTEXT_MENU, wxContextMenuEventHandler( guAlbumBrowserItemPanel::OnContextMenu ), NULL, this );

    Disconnect( ID_LASTFM_SEARCH_LINK, ID_LASTFM_SEARCH_LINK + 999, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnSearchLinkClicked ) );

    Disconnect( ID_ALBUMBROWSER_PLAY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnPlayClicked ), NULL, this );
    Disconnect( ID_ALBUMBROWSER_ENQUEUE, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnEnqueueClicked ), NULL, this );
    Disconnect( ID_ALBUMBROWSER_COPYTOCLIPBOARD, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnCopyToClipboard ), NULL, this );
    Disconnect( ID_ALBUM_SELECTNAME, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnAlbumSelectName ), NULL, this );
    Disconnect( ID_ARTIST_SELECTNAME, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowserItemPanel::OnArtistSelectName ), NULL, this );
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::SetAlbumItem( const int index, guAlbumBrowserItem * albumitem, wxBitmap * blankcd )
{
    m_Index = index;
    m_AlbumBrowserItem = albumitem;
    if( m_AlbumBrowserItem )
    {
        //guLogMessage( wxT( "Inserting item %i '%s'" ), index, albumitem->m_AlbumName.c_str() );
        if( m_AlbumBrowserItem->m_CoverBitmap )
            m_Bitmap->SetBitmap( * m_AlbumBrowserItem->m_CoverBitmap );
        else
            m_Bitmap->SetBitmap( * blankcd );

        m_ArtistLabel->SetLabel( m_AlbumBrowserItem->m_ArtistName );
        m_AlbumLabel->SetLabel( m_AlbumBrowserItem->m_AlbumName );

        m_TracksLabel->SetLabel( wxString::Format( wxT( "(%u) %u " ),
            m_AlbumBrowserItem->m_Year,
            m_AlbumBrowserItem->m_TrackCount ) + _( "Tracks" ) );
    }
    else
    {
        m_Bitmap->SetBitmap( * blankcd );
        m_ArtistLabel->SetLabel( wxEmptyString );
        m_AlbumLabel->SetLabel( wxEmptyString );
        m_TracksLabel->SetLabel( wxEmptyString );
    }
    //Layout();
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::UpdateDetails( void )
{
    if( m_AlbumBrowserItem )
    {
        m_TracksLabel->SetLabel( wxString::Format( wxT( "(%u) %u " ),
            m_AlbumBrowserItem->m_Year,
            m_AlbumBrowserItem->m_TrackCount ) + _( "Tracks" ) );
    }
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::OnContextMenu( wxContextMenuEvent &event )
{
    if( !m_AlbumBrowserItem )
        return;

    wxMenu Menu;
    wxMenuItem * MenuItem;
    wxPoint Point = event.GetPosition();
    // If from keyboard
    if( Point.x == -1 && Point.y == -1 )
    {
        wxSize Size = GetSize();
        Point.x = Size.x / 2;
        Point.y = Size.y / 2;
    }
    else
    {
        Point = ScreenToClient( Point );
    }


    if( m_AlbumBrowserItem->m_AlbumId != ( size_t ) wxNOT_FOUND )
    {
        MenuItem = new wxMenuItem( &Menu, ID_ALBUMBROWSER_PLAY, _( "Play" ), _( "Play the album tracks" ) );
        MenuItem->SetBitmap( guImage( guIMAGE_INDEX_tiny_playback_start ) );
        Menu.Append( MenuItem );

        MenuItem = new wxMenuItem( &Menu, ID_ALBUMBROWSER_ENQUEUE, _( "Enqueue" ), _( "Enqueue the album tracks to the playlist" ) );
        MenuItem->SetBitmap( guImage( guIMAGE_INDEX_tiny_add ) );
        Menu.Append( MenuItem );

        Menu.AppendSeparator();

        MenuItem = new wxMenuItem( &Menu, ID_ALBUMBROWSER_EDITLABELS, _( "Edit Labels" ), _( "Edit the labels assigned to the selected albums" ) );
        Menu.Append( MenuItem );

        MenuItem = new wxMenuItem( &Menu, ID_ALBUMBROWSER_EDITTRACKS, _( "Edit Album songs" ), _( "Edit the selected albums songs" ) );
        Menu.Append( MenuItem );

        Menu.AppendSeparator();

        MenuItem = new wxMenuItem( &Menu, ID_ALBUM_SELECTNAME, _( "Search Album" ), _( "Search the album in the library" ) );
        MenuItem->SetBitmap( guImage( guIMAGE_INDEX_tiny_search ) );
        Menu.Append( MenuItem );

        MenuItem = new wxMenuItem( &Menu, ID_ARTIST_SELECTNAME, _( "Search Artist" ), _( "Search the artist in the library" ) );
        MenuItem->SetBitmap( guImage( guIMAGE_INDEX_tiny_search ) );
        Menu.Append( MenuItem );

        Menu.AppendSeparator();

        MenuItem = new wxMenuItem( &Menu, ID_ALBUMBROWSER_SEARCHCOVER, _( "Download Album cover" ), _( "Download cover for the current selected album" ) );
        MenuItem->SetBitmap( guImage( guIMAGE_INDEX_download_covers ) );
        Menu.Append( MenuItem );

        MenuItem = new wxMenuItem( &Menu, ID_ALBUMBROWSER_SELECTCOVER, _( "Select cover location" ), _( "Select the cover image file from disk" ) );
        MenuItem->SetBitmap( guImage( guIMAGE_INDEX_download_covers ) );
        Menu.Append( MenuItem );

        MenuItem = new wxMenuItem( &Menu, ID_ALBUMBROWSER_DELETECOVER, _( "Delete Album cover" ), _( "Delete the cover for the selected album" ) );
        MenuItem->SetBitmap( guImage( guIMAGE_INDEX_edit_delete ) );
        Menu.Append( MenuItem );

        Menu.AppendSeparator();

        MenuItem = new wxMenuItem( &Menu, ID_ALBUMBROWSER_COPYTOCLIPBOARD, wxT( "Copy to clipboard" ), _( "Copy the album info to clipboard" ) );
        MenuItem->SetBitmap( guImage( guIMAGE_INDEX_tiny_edit_copy ) );
        Menu.Append( MenuItem );

        Menu.AppendSeparator();

        MenuItem = new wxMenuItem( &Menu, ID_ALBUMBROWSER_COPYTO, _( "Copy to..." ), _( "Copy the current album to a directory or device" ) );
        Menu.Append( MenuItem );

        Menu.AppendSeparator();

        AddAlbumCommands( &Menu, 1 );
        AddOnlineLinksMenu( &Menu );
    }

    // Add Links and Commands
    PopupMenu( &Menu, Point.x, Point.y );
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::OnCommandClicked( wxCommandEvent &event )
{
    m_AlbumBrowser->OnCommandClicked( event.GetId(), m_AlbumBrowserItem->m_AlbumId );
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::OnSearchLinkClicked( wxCommandEvent &event )
{
    int index = event.GetId();

    guConfig * Config = ( guConfig * ) Config->Get();
    if( Config )
    {
        wxArrayString Links = Config->ReadAStr( wxT( "Link" ), wxEmptyString, wxT( "SearchLinks" ) );
        wxASSERT( Links.Count() > 0 );

        index -= ID_LASTFM_SEARCH_LINK;
        wxString SearchLink = Links[ index ];
        wxString Lang = Config->ReadStr( wxT( "Language" ), wxT( "en" ), wxT( "LastFM" ) );
        if( Lang.IsEmpty() )
        {
            Lang = ( ( guMainApp * ) wxTheApp )->GetLocale()->GetCanonicalName().Mid( 0, 2 );
            //guLogMessage( wxT( "Locale: %s" ), ( ( guMainApp * ) wxTheApp )->GetLocale()->GetCanonicalName().c_str() );
        }
        SearchLink.Replace( wxT( "{lang}" ), Lang );
        SearchLink.Replace( wxT( "{text}" ), guURLEncode( m_AlbumBrowserItem->m_ArtistName + wxT( " " ) +
                                                          m_AlbumBrowserItem->m_AlbumName ) );
        guWebExecute( SearchLink );
    }
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::OnPlayClicked( wxCommandEvent &event )
{
    m_AlbumBrowser->SelectAlbum( m_AlbumBrowserItem->m_AlbumId, false );
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::OnEnqueueClicked( wxCommandEvent &event )
{
    m_AlbumBrowser->SelectAlbum( m_AlbumBrowserItem->m_AlbumId, true );
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::OnCopyToClipboard( wxCommandEvent &event )
{
    if( wxTheClipboard->Open() )
    {
        wxTheClipboard->Clear();
        if( !wxTheClipboard->AddData( new wxTextDataObject(
            m_AlbumBrowserItem->m_ArtistName + wxT( " " ) + m_AlbumBrowserItem->m_AlbumName ) ) )
        {
            guLogError( wxT( "Can't copy data to the clipboard" ) );
        }
        wxTheClipboard->Close();
    }
    else
    {
        guLogError( wxT( "Could not open the clipboard object" ) );
    }
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::OnAlbumSelectName( wxCommandEvent &event )
{
    wxString * AlbumName = new wxString( m_AlbumBrowserItem->m_AlbumName );
    wxCommandEvent evt( wxEVT_COMMAND_MENU_SELECTED, ID_ALBUM_SELECTNAME );
    evt.SetClientData( ( void * ) AlbumName );
    wxPostEvent( wxTheApp->GetTopWindow(), evt );
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::OnArtistSelectName( wxCommandEvent &event )
{
    wxString * ArtistName = new wxString( m_AlbumBrowserItem->m_ArtistName );
    wxCommandEvent evt( wxEVT_COMMAND_MENU_SELECTED, ID_ARTIST_SELECTNAME );
    evt.SetClientData( ( void * ) ArtistName );
    wxPostEvent( wxTheApp->GetTopWindow(), evt );
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::OnAlbumDownloadCoverClicked( wxCommandEvent &event )
{
    m_AlbumBrowser->OnAlbumDownloadCoverClicked( m_AlbumBrowserItem->m_AlbumId );
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::OnAlbumSelectCoverClicked( wxCommandEvent &event )
{
    m_AlbumBrowser->OnAlbumSelectCoverClicked( m_AlbumBrowserItem->m_AlbumId );
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::OnAlbumDeleteCoverClicked( wxCommandEvent &event )
{
    m_AlbumBrowser->OnAlbumDeleteCoverClicked( m_AlbumBrowserItem->m_AlbumId );
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::OnAlbumCopyToClicked( wxCommandEvent &event )
{
    m_AlbumBrowser->OnAlbumCopyToClicked( m_AlbumBrowserItem->m_AlbumId );
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::OnAlbumEditLabelsClicked( wxCommandEvent &event )
{
    m_AlbumBrowser->OnAlbumEditLabelsClicked( m_AlbumBrowserItem->m_AlbumId );
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowserItemPanel::OnAlbumEditTracksClicked( wxCommandEvent &event )
{
    m_AlbumBrowser->OnAlbumEditTracksClicked( m_AlbumBrowserItem->m_AlbumId );
}



// -------------------------------------------------------------------------------- //
// guAlbumBrowser
// -------------------------------------------------------------------------------- //
guAlbumBrowser::guAlbumBrowser( wxWindow * parent, guDbLibrary * db, guPlayerPanel * playerpanel ) :
    wxPanel( parent, wxID_ANY )
{
    m_Db = db;
    m_PlayerPanel = playerpanel;
    m_UpdateDetails = NULL;
    m_ItemStart = 0;
    m_LastItemStart = wxNOT_FOUND;
    m_ItemCount = 1;
    wxImage TmpImg = wxImage( guImage( guIMAGE_INDEX_blank_cd_cover ) );
    TmpImg.Rescale( 100, 100 );
    m_BlankCD = new wxBitmap( TmpImg );

    // GUI
	SetMinSize( wxSize( 120, 150 ) );

	wxBoxSizer* MainSizer;
	MainSizer = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer * FilterSizer;
	FilterSizer = new wxBoxSizer( wxHORIZONTAL );

//	wxString m_FilterChoiceChoices[] = { _( "Filter By" ), _("Label"), _( "Genre" ), _("Artist"), _("Year"), _("Ratting") };
//	int m_FilterChoiceNChoices = sizeof( m_FilterChoiceChoices ) / sizeof( wxString );
//	m_FilterChoice = new wxChoice( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_FilterChoiceNChoices, m_FilterChoiceChoices, 0 );
//	m_FilterChoice->SetSelection( 0 );
//	FilterSizer->Add( m_FilterChoice, 0, wxTOP|wxRIGHT|wxLEFT, 5 );
	m_FilterBtn = new wxToggleButton( this, wxID_ANY, _( "Filter" ), wxDefaultPosition, wxDefaultSize, 0 );
	FilterSizer->Add( m_FilterBtn, 0, wxTOP|wxLEFT, 5 );

	m_EditFilterBtn = new wxBitmapButton( this, wxID_ANY, guImage( guIMAGE_INDEX_tiny_search ), wxDefaultPosition, wxSize( 28, 28 ), wxBU_AUTODRAW );
	FilterSizer->Add( m_EditFilterBtn, 0, wxTOP|wxRIGHT|wxLEFT, 5 );

	FilterSizer->Add( 0, 0, 1, wxEXPAND, 5 );

	wxString m_OrderChoiceChoices[] = { _("Order By"), _("Name"), _("Year"), _("Year Desc."), _("Artist, Name"), _("Artist, Year"), _("Artist, Year Desc.") };
	int m_OrderChoiceNChoices = sizeof( m_OrderChoiceChoices ) / sizeof( wxString );
	m_OrderChoice = new wxChoice( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_OrderChoiceNChoices, m_OrderChoiceChoices, 0 );
	m_OrderChoice->SetSelection( 0 );
	FilterSizer->Add( m_OrderChoice, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxRIGHT|wxLEFT, 5 );

	MainSizer->Add( FilterSizer, 0, wxEXPAND, 5 );


	m_AlbumsSizer = new wxGridSizer( 1, 1, 0, 0 );

	m_ItemPanels.Add( new guAlbumBrowserItemPanel( this, 0 ) );

	m_AlbumsSizer->Add( m_ItemPanels[ 0 ], 1, wxEXPAND|wxALL, 5 );

	MainSizer->Add( m_AlbumsSizer, 1, wxEXPAND, 5 );

	wxBoxSizer* NavigatorSizer;
	NavigatorSizer = new wxBoxSizer( wxVERTICAL );

	m_NavLabel = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_NavLabel->Wrap( -1 );
	NavigatorSizer->Add( m_NavLabel, 0, wxRIGHT|wxLEFT|wxEXPAND, 5 );

	m_NavSlider = new wxSlider( this, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL );
	NavigatorSizer->Add( m_NavSlider, 1, wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxBOTTOM|wxRIGHT|wxLEFT, 5 );

	MainSizer->Add( NavigatorSizer, 0, wxEXPAND, 5 );

	this->SetSizer( MainSizer );
	this->Layout();

    m_NavSlider->SetFocus();

    Connect( wxEVT_SIZE, wxSizeEventHandler( guAlbumBrowser::OnChangedSize ), NULL, this );

	m_FilterBtn->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( guAlbumBrowser::OnFilterSelected ), NULL, this );
	m_EditFilterBtn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( guAlbumBrowser::OnEditFilterClicked ), NULL, this );
	m_OrderChoice->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( guAlbumBrowser::OnOrderSelected ), NULL, this );

	m_NavSlider->Connect( wxEVT_SCROLL_CHANGED, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
	m_NavSlider->Connect( wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
//	m_NavSlider->Connect( wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
//	m_NavSlider->Connect( wxEVT_SCROLL_TOP, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
//	m_NavSlider->Connect( wxEVT_SCROLL_BOTTOM, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
//	m_NavSlider->Connect( wxEVT_SCROLL_LINEUP, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
//	m_NavSlider->Connect( wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
//	m_NavSlider->Connect( wxEVT_SCROLL_PAGEUP, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
//	m_NavSlider->Connect( wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );

	Connect( wxEVT_TIMER, wxTimerEventHandler( guAlbumBrowser::OnRefreshTimer ), NULL, this );

	Connect( ID_ALBUMBROWSER_UPDATEDETAILS, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowser::OnUpdateDetails ), NULL, this );

    RefreshCount();
    //ReloadItems();
    m_RefreshTimer.SetOwner( this );
}

// -------------------------------------------------------------------------------- //
guAlbumBrowser::~guAlbumBrowser()
{
    if( m_BlankCD )
        delete m_BlankCD;

    Disconnect( wxEVT_SIZE, wxSizeEventHandler( guAlbumBrowser::OnChangedSize ), NULL, this );

	m_FilterBtn->Disconnect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( guAlbumBrowser::OnFilterSelected ), NULL, this );
	m_OrderChoice->Disconnect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( guAlbumBrowser::OnOrderSelected ), NULL, this );

	m_NavSlider->Disconnect( wxEVT_SCROLL_CHANGED, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
	m_NavSlider->Disconnect( wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
//	m_NavSlider->Disconnect( wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
//	m_NavSlider->Disconnect( wxEVT_SCROLL_TOP, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
//	m_NavSlider->Disconnect( wxEVT_SCROLL_BOTTOM, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
//	m_NavSlider->Disconnect( wxEVT_SCROLL_LINEUP, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
//	m_NavSlider->Disconnect( wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
//	m_NavSlider->Disconnect( wxEVT_SCROLL_PAGEUP, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );
//	m_NavSlider->Disconnect( wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler( guAlbumBrowser::OnChangingPosition ), NULL, this );

	Disconnect( wxEVT_TIMER, wxTimerEventHandler( guAlbumBrowser::OnRefreshTimer ), NULL, this );

	Disconnect( ID_ALBUMBROWSER_UPDATEDETAILS, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( guAlbumBrowser::OnUpdateDetails ), NULL, this );
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::OnChangedSize( wxSizeEvent &event )
{
    wxSize Size = event.GetSize();
    if( Size != m_LastSize )
    {
        size_t ColItems = Size.GetWidth() / 220;
        size_t RowItems = Size.GetHeight() / 220;
        guLogMessage( wxT( "Row: %i  Col:%i" ), RowItems, ColItems );
        if( ColItems * RowItems != m_ItemPanels.Count() )
        {
            size_t OldCount = m_ItemPanels.Count();
            m_ItemCount = ColItems * RowItems;
            guLogMessage( wxT( "We need to reassign the panels from %i to %i" ), OldCount, m_ItemCount );
            if( OldCount != m_ItemCount )
            {
                m_AlbumsSizer->SetCols( ColItems );
                m_AlbumsSizer->SetRows( RowItems );

                if( OldCount < m_ItemCount )
                {
                    while( m_ItemPanels.Count() < m_ItemCount )
                    {
                        int Index = m_ItemPanels.Count();
                        m_ItemPanels.Add( new guAlbumBrowserItemPanel( this, Index ) );
                        m_AlbumsSizer->Add( m_ItemPanels[ Index ], 1, wxEXPAND|wxALL, 5 );
                    }
                }
                else //if( m_ItemCount < OldCount )
                {
                    size_t Index;
                    //size_t Count;
                    for( Index = OldCount; Index > m_ItemCount; Index-- )
                    {
                        guAlbumBrowserItemPanel *  ItemPanel = m_ItemPanels[ Index - 1 ];
                        m_AlbumsSizer->Detach( ItemPanel );
                        m_ItemPanels.RemoveAt( Index - 1 );
                        delete ItemPanel;
                    }
                }

                RefreshPageCount();
                ReloadItems();
                RefreshAll();
            }
        }

        m_LastSize = Size;
    }
    event.Skip();
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::ReloadItems( void )
{
    m_AlbumItems.Empty();
    m_Db->GetAlbums( &m_AlbumItems, m_FilterBtn->GetValue() ? &m_DynPlayList : NULL,
                     m_ItemStart, m_ItemCount, m_OrderChoice->GetSelection() - 1 );
//    guLogMessage( wxT( "Read %i items from %i (%i)" ), m_AlbumItems.Count(), m_ItemStart, m_ItemCount );
//    RefreshAll();
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::RefreshAll( void )
{
    size_t Index;
    size_t Count = m_ItemCount;
    for( Index = 0; Index < Count; Index++ )
    {
        //guLogMessage( wxT( "%i %s " ), Index, m_AlbumItems[ Index ].m_AlbumName.c_str() );
        if( Index < m_AlbumItems.Count() )
            m_ItemPanels[ Index ]->SetAlbumItem( Index, &m_AlbumItems[ Index ], m_BlankCD );
        else
            m_ItemPanels[ Index ]->SetAlbumItem( Index, NULL, m_BlankCD );
    }
    Layout();

    if( m_UpdateDetails )
    {
        m_UpdateDetails->Pause();
        m_UpdateDetails->Delete();
    }

    m_UpdateDetails = new guUpdateAlbumDetails( this, &m_UpdateDetails );
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::OnChangingPosition( wxScrollEvent& event )
{
    int CurPage = event.GetPosition(); //( ( event.GetPosition() * m_PagesCount ) / 100 );
    m_ItemStart = CurPage * m_ItemCount;
    guLogMessage( wxT( "ChangePosition: %i -> %i     Albums(%i / %i)" ), m_ItemStart, m_LastItemStart, CurPage, m_AlbumsCount );
    if( m_LastItemStart != m_ItemStart )
    {
        if( m_RefreshTimer.IsRunning() )
            m_RefreshTimer.Stop();

        m_RefreshTimer.Start( guALBUMBROWSER_REFRESH_DELAY, wxTIMER_ONE_SHOT );

        UpdateNavLabel( CurPage );
//        ReloadItems();
//        RefreshAll();

        m_LastItemStart = m_ItemStart;
    }
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::OnRefreshTimer( wxTimerEvent &event )
{
    ReloadItems();
    RefreshAll();
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::SelectAlbum( const int albumid, const bool append )
{
    guTrackArray Tracks;
    wxArrayInt Selections;
    Selections.Add( albumid );
    if( m_Db->GetAlbumsSongs( Selections, &Tracks ) )
    {
        if( append )
            m_PlayerPanel->AddToPlayList( Tracks );
        else
            m_PlayerPanel->SetPlayList( Tracks );
    }
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::OnEditFilterClicked( wxCommandEvent &event )
{
    guDynPlayListEditor * PlayListEditor = new guDynPlayListEditor( this, &m_DynPlayList, true );
    if( PlayListEditor->ShowModal() == wxID_OK )
    {
        PlayListEditor->FillPlayListEditData();

        if( m_DynPlayList.m_Filters.Count() )
        {
            RefreshCount();
            ReloadItems();
            m_LastItemStart = wxNOT_FOUND;
            m_NavSlider->SetValue( 0 );
            RefreshAll();
        }
        else
        {
            m_FilterBtn->SetValue( false );
        }
    }
    else
    {
        m_FilterBtn->SetValue( false );
    }
    PlayListEditor->Destroy();
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::OnFilterSelected( wxCommandEvent &event )
{
    // If its enabled
    if( event.GetInt() )
    {
        OnEditFilterClicked( event );
    }
    else
    {
        RefreshCount();
        ReloadItems();
        m_LastItemStart = wxNOT_FOUND;
        m_NavSlider->SetValue( 0 );
        RefreshAll();
    }
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::OnOrderSelected( wxCommandEvent &event )
{
    ReloadItems();
    m_LastItemStart = wxNOT_FOUND;
    m_NavSlider->SetValue( 0 );
    RefreshAll();
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::OnCommandClicked( const int cmdid, const int albumid )
{
    int index;
    int count;
    wxArrayInt Selection;
    Selection.Add( albumid );
    if( Selection.Count() )
    {
        index = cmdid;

        guConfig * Config = ( guConfig * ) Config->Get();
        if( Config )
        {
            wxArrayString Commands = Config->ReadAStr( wxT( "Cmd" ), wxEmptyString, wxT( "Commands" ) );
            wxASSERT( Commands.Count() > 0 );

            index -= ID_ALBUM_COMMANDS;
            wxString CurCmd = Commands[ index ];
            if( CurCmd.Find( wxT( "{bp}" ) ) != wxNOT_FOUND )
            {
                wxArrayString AlbumPaths = m_Db->GetAlbumsPaths( Selection );
                wxString Paths = wxEmptyString;
                count = AlbumPaths.Count();
                for( index = 0; index < count; index++ )
                {
                    Paths += wxT( " \"" ) + AlbumPaths[ index ] + wxT( "\"" );
                }
                CurCmd.Replace( wxT( "{bp}" ), Paths.Trim( false ) );
            }

            if( CurCmd.Find( wxT( "{bc}" ) ) != wxNOT_FOUND )
            {
                int CoverId = m_Db->GetAlbumCoverId( Selection[ 0 ] );
                wxString CoverPath = wxEmptyString;
                if( CoverId > 0 )
                {
                    CoverPath = wxT( "\"" ) + m_Db->GetCoverPath( CoverId ) + wxT( "\"" );
                }
                CurCmd.Replace( wxT( "{bc}" ), CoverPath );
            }

            if( CurCmd.Find( wxT( "{tp}" ) ) != wxNOT_FOUND )
            {
                guTrackArray Songs;
                wxString SongList = wxEmptyString;
                if( m_Db->GetAlbumsSongs( Selection, &Songs ) )
                {
                    count = Songs.Count();
                    for( index = 0; index < count; index++ )
                    {
                        SongList += wxT( " \"" ) + Songs[ index ].m_FileName + wxT( "\"" );
                    }
                    CurCmd.Replace( wxT( "{tp}" ), SongList.Trim( false ) );
                }
            }

            //guLogMessage( wxT( "Execute Command '%s'" ), CurCmd.c_str() );
            guExecute( CurCmd );
        }
    }
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::OnAlbumDownloadCoverClicked( const int albumid )
{
    wxArrayInt Albums;
    Albums.Add( albumid );
    if( Albums.Count() )
    {
        wxString AlbumName;
        wxString ArtistName;
        wxString AlbumPath;
        if( !m_Db->GetAlbumInfo( Albums[ 0 ], &AlbumName, &ArtistName, &AlbumPath ) )
        {
            wxMessageBox( _( "Could not find the Album in the songs library.\n"\
                             "You should update the library." ), _( "Error" ), wxICON_ERROR | wxOK );
            return;
        }

        AlbumName = RemoveSearchFilters( AlbumName );

        guCoverEditor * CoverEditor = new guCoverEditor( this, ArtistName, AlbumName );
        if( CoverEditor )
        {
            if( CoverEditor->ShowModal() == wxID_OK )
            {
                //guLogMessage( wxT( "About to download cover from selected url" ) );
                wxImage * CoverImage = CoverEditor->GetSelectedCoverImage();
                CoverImage->SaveFile( AlbumPath + wxT( "cover.jpg" ), wxBITMAP_TYPE_JPEG );
                m_Db->SetAlbumCover( Albums[ 0 ], AlbumPath + wxT( "cover.jpg" ) );

                ReloadItems();
                RefreshAll();
            }
            CoverEditor->Destroy();
        }
    }
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::OnAlbumSelectCoverClicked( const int albumid )
{
    wxArrayInt Albums;
    Albums.Add( albumid );
    if( Albums.Count() )
    {
        wxString AlbumName;
        wxString ArtistName;
        wxString AlbumPath;
        if( !m_Db->GetAlbumInfo( Albums[ 0 ], &AlbumName, &ArtistName, &AlbumPath ) )
        {
            wxMessageBox( _( "Could not find the Album in the songs library.\n"\
                             "You should update the library." ), _( "Error" ), wxICON_ERROR | wxOK );
            return;
        }

        wxFileDialog * FileDialog = new wxFileDialog( this,
            wxT( "Select the cover filename" ),
            AlbumPath,
            wxT( "cover.jpg" ),
            wxT( "*.jpg;*.png" ),
            wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_PREVIEW );

        if( FileDialog )
        {
            if( FileDialog->ShowModal() == wxID_OK )
            {
                m_Db->SetAlbumCover( Albums[ 0 ], FileDialog->GetPath() );
                ReloadItems();
                RefreshAll();
            }
            FileDialog->Destroy();
        }
    }
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::OnAlbumDeleteCoverClicked( const int albumid )
{
    wxArrayInt Albums;
    Albums.Add( albumid );
    if( Albums.Count() )
    {
        if( wxMessageBox( _( "Are you sure to delete the selected album cover?" ),
                          _( "Confirm" ),
                          wxICON_QUESTION | wxYES_NO | wxCANCEL, this ) == wxYES )
        {
            int CoverId = m_Db->GetAlbumCoverId( Albums[ 0 ] );
            if( CoverId > 0 )
            {
                wxString CoverPath = m_Db->GetCoverPath( CoverId );
                wxASSERT( !CoverPath.IsEmpty() );
                if( !wxRemoveFile( CoverPath ) )
                {
                    guLogError( wxT( "Could not remove the cover file '%s'" ), CoverPath.c_str() );
                }
            }
            m_Db->SetAlbumCover( Albums[ 0 ], wxEmptyString );
            ReloadItems();
            RefreshAll();
        }
    }
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::OnAlbumCopyToClicked( const int albumid )
{
    guTrackArray * Tracks = new guTrackArray();
    wxArrayInt Albums;
    Albums.Add( albumid );

    m_Db->GetAlbumsSongs( Albums, Tracks );

    wxCommandEvent event( wxEVT_COMMAND_MENU_SELECTED, ID_MAINFRAME_COPYTO );
    event.SetClientData( ( void * ) Tracks );
    wxPostEvent( wxTheApp->GetTopWindow(), event );
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::OnAlbumEditLabelsClicked( const int albumid )
{
    guListItems Labels;
    wxArrayInt  Albums;

    m_Db->GetLabels( &Labels, true );

    Albums.Add( albumid );
    guLabelEditor * LabelEditor = new guLabelEditor( this, m_Db, _( "Albums Labels Editor" ), false,
                                         Labels, m_Db->GetAlbumsLabels( Albums ) );
    if( LabelEditor )
    {
        if( LabelEditor->ShowModal() == wxID_OK )
        {
            m_Db->UpdateAlbumsLabels( Albums, LabelEditor->GetCheckedIds() );
        }
        LabelEditor->Destroy();
    }
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::OnAlbumEditTracksClicked( const int albumid )
{
    guTrackArray Songs;
    guImagePtrArray Images;
    wxArrayString Lyrics;
    //m_AlbumListCtrl->GetSelectedSongs( &Songs );
    wxArrayInt Albums;
    Albums.Add( albumid );
    m_Db->GetAlbumsSongs( Albums, &Songs, true );
    if( !Songs.Count() )
        return;
    guTrackEditor * TrackEditor = new guTrackEditor( this, m_Db, &Songs, &Images, &Lyrics );
    if( TrackEditor )
    {
        if( TrackEditor->ShowModal() == wxID_OK )
        {
            m_Db->UpdateSongs( &Songs );
            UpdateImages( Songs, Images );
            UpdateLyrics( Songs, Lyrics );
            m_PlayerPanel->UpdatedTracks( &Songs );
        }
        TrackEditor->Destroy();
    }
}

// -------------------------------------------------------------------------------- //
void guAlbumBrowser::OnUpdateDetails( wxCommandEvent &event )
{
    //guLogMessage( wxT( "OnUpdateDetails %i" ), event.GetInt() );
    m_ItemPanels[ event.GetInt() ]->UpdateDetails();
}

// -------------------------------------------------------------------------------- //
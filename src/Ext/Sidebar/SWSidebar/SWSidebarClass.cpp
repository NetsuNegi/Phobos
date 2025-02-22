#include "SWSidebarClass.h"
#include <CommandClass.h>

#include <Ext/House/Body.h>
#include <Ext/Side/Body.h>
#include <Ext/SWType/Body.h>

SWSidebarClass SWSidebarClass::Instance;
CommandClass* SWSidebarClass::Commands[10];

// =============================
// functions

bool SWSidebarClass::AddColumn()
{
	auto& columns = this->Columns;

	if (static_cast<int>(columns.size()) >= Phobos::UI::SuperWeaponSidebar_MaxColumns)
		return false;

	const int maxButtons = Phobos::UI::SuperWeaponSidebar_Max - static_cast<int>(columns.size());

	if (maxButtons <= 0)
		return false;

	const int cameoWidth = 60;
	const auto column = GameCreate<SWColumnClass>(SWButtonClass::StartID + SuperWeaponTypeClass::Array->Count + 1 + static_cast<int>(columns.size()), maxButtons, 0, 0, cameoWidth + Phobos::UI::SuperWeaponSidebar_Interval, Phobos::UI::SuperWeaponSidebar_CameoHeight);

	if (!column)
		return false;

	column->Zap();
	GScreenClass::Instance->AddButton(column);
	return true;
}

bool SWSidebarClass::RemoveColumn()
{
	auto& columns = this->Columns;

	if (columns.empty())
		return false;

	if (const auto backColumn = columns.back())
	{
		AnnounceInvalidPointer(SWSidebarClass::Instance.CurrentColumn, backColumn);
		GScreenClass::Instance->RemoveButton(backColumn);

		columns.erase(columns.end() - 1);
		return true;
	}

	return false;
}

void SWSidebarClass::InitClear()
{
	this->CurrentColumn = nullptr;
	this->CurrentButton = nullptr;

	if (const auto toggleButton = this->ToggleButton)
	{
		this->ToggleButton = nullptr;
		GScreenClass::Instance->RemoveButton(toggleButton);
	}

	auto& columns = this->Columns;

	for (const auto column : columns)
	{
		column->ClearButtons();
		GScreenClass::Instance->RemoveButton(column);
	}

	columns.clear();
}

void SWSidebarClass::InitIO()
{
	if (!Phobos::UI::SuperWeaponSidebar || Unsorted::ArmageddonMode)
		return;

	if (const auto pSideExt = SideExt::ExtMap.Find(SideClass::Array->Items[ScenarioClass::Instance->PlayerSideIndex]))
	{
		const auto pOnPCX = pSideExt->SuperWeaponSidebar_OnPCX.GetSurface();
		const auto pOffPCX = pSideExt->SuperWeaponSidebar_OffPCX.GetSurface();
		int width = 0, height = 0;

		if (pOnPCX)
		{
			if (pOffPCX)
			{
				width = std::max(pOnPCX->GetWidth(), pOffPCX->GetWidth());
				height = std::max(pOnPCX->GetHeight(), pOffPCX->GetHeight());
			}
			else
			{
				width = pOnPCX->GetWidth();
				height = pOnPCX->GetHeight();
			}
		}
		else if (pOffPCX)
		{
			width = pOffPCX->GetWidth();
			height = pOffPCX->GetHeight();
		}

		if (width > 0 && height > 0)
		{
			if (const auto toggleButton = GameCreate<ToggleSWButtonClass>(SWButtonClass::StartID + SuperWeaponTypeClass::Array->Count, 0, 0, width, height))
			{
				toggleButton->Zap();
				GScreenClass::Instance->AddButton(toggleButton);
				SWSidebarClass::Instance.ToggleButton = toggleButton;
				toggleButton->UpdatePosition();
			}
		}
	}

	for (const auto superIdx : SidebarExt::Global()->SWSidebar_Indices)
		SWSidebarClass::Instance.AddButton(superIdx);
}

bool SWSidebarClass::AddButton(int superIdx)
{
	if (this->DisableEntry)
		return false;

	auto& columns = this->Columns;

	if (columns.empty() && !this->AddColumn())
		return false;

	if (std::any_of(columns.begin(), columns.end(), [superIdx](SWColumnClass* column) { return std::any_of(column->Buttons.begin(), column->Buttons.end(), [superIdx](SWButtonClass* button) { return button->SuperIndex == superIdx; }); }))
		return true;

	const bool success = columns.back()->AddButton(superIdx);

	if (success)
		SidebarExt::Global()->SWSidebar_Indices.emplace_back(superIdx);

	return success;
}

void SWSidebarClass::SortButtons()
{
	auto& columns = this->Columns;

	if (columns.empty())
	{
		if (const auto toggleButton = this->ToggleButton)
			toggleButton->UpdatePosition();

		return;
	}

	std::vector<SWButtonClass*> vec_Buttons;
	vec_Buttons.reserve(this->GetMaximumButtonCount());

	for (const auto column : columns)
	{
		for (const auto button : column->Buttons)
			vec_Buttons.emplace_back(button);

		column->ClearButtons(false);
	}

	const unsigned int ownerBits = 1u << HouseClass::CurrentPlayer->Type->ArrayIndex;

	std::stable_sort(vec_Buttons.begin(), vec_Buttons.end(), [ownerBits](SWButtonClass* const a, SWButtonClass* const b)
		{
			const auto pExtA = SWTypeExt::ExtMap.Find(SuperWeaponTypeClass::Array->GetItemOrDefault(a->SuperIndex));
			const auto pExtB = SWTypeExt::ExtMap.Find(SuperWeaponTypeClass::Array->GetItemOrDefault(b->SuperIndex));

			if (pExtB && (pExtB->SuperWeaponSidebar_PriorityHouses & ownerBits) && (!pExtA || !(pExtA->SuperWeaponSidebar_PriorityHouses & ownerBits)))
				return false;

			if ((!pExtB || !(pExtB->SuperWeaponSidebar_PriorityHouses & ownerBits)) && pExtA && (pExtA->SuperWeaponSidebar_PriorityHouses & ownerBits))
				return true;

			return BuildType::SortsBefore(AbstractType::Special, a->SuperIndex, AbstractType::Special, b->SuperIndex);
		});

	const int buttonCount = static_cast<int>(vec_Buttons.size());
	const int cameoWidth = 60, cameoHeight = 48;
	const int maximum = Phobos::UI::SuperWeaponSidebar_Max;
	const int cameoHarfInterval = (Phobos::UI::SuperWeaponSidebar_CameoHeight - cameoHeight) / 2;
	int location_Y = (DSurface::ViewBounds().Height - std::min(buttonCount, maximum) * Phobos::UI::SuperWeaponSidebar_CameoHeight) / 2;
	Point2D location = { Phobos::UI::SuperWeaponSidebar_LeftOffset, location_Y + cameoHarfInterval };
	int rowIdx = 0, columnIdx = 0;

	for (const auto button : vec_Buttons)
	{
		const auto column = columns[columnIdx];

		if (rowIdx == 0)
		{
			const auto pTopPCX = SideExt::ExtMap.Find(SideClass::Array->Items[ScenarioClass::Instance->PlayerSideIndex])->SuperWeaponSidebar_TopPCX.GetSurface();
			column->SetPosition(location.X - Phobos::UI::SuperWeaponSidebar_LeftOffset, location_Y - (pTopPCX ? pTopPCX->GetHeight() : 0));
		}

		column->Buttons.emplace_back(button);
		button->SetColumn(columnIdx);
		button->SetPosition(location.X, location.Y);
		rowIdx++;

		if (rowIdx >= maximum - columnIdx)
		{
			rowIdx = 0;
			columnIdx++;
			location_Y += Phobos::UI::SuperWeaponSidebar_CameoHeight / 2;
			location.X += cameoWidth + Phobos::UI::SuperWeaponSidebar_Interval;
			location.Y = location_Y + cameoHarfInterval;
		}
		else
		{
			location.Y += Phobos::UI::SuperWeaponSidebar_CameoHeight;
		}
	}

	for (const auto column : columns)
		column->SetHeight(column->Buttons.size() * Phobos::UI::SuperWeaponSidebar_CameoHeight);
}

int SWSidebarClass::GetMaximumButtonCount()
{
	const int firstColumn = Phobos::UI::SuperWeaponSidebar_Max;
	const int columns = std::min(firstColumn, Phobos::UI::SuperWeaponSidebar_MaxColumns);
	return (firstColumn + (firstColumn - (columns - 1))) * columns / 2;
}

bool SWSidebarClass::IsEnabled()
{
	return SidebarExt::Global()->SWSidebar_Enable;
}
// Hooks

DEFINE_HOOK(0x4F92FB, HouseClass_UpdateTechTree_SWSidebar, 0x7)
{
	enum { SkipGameCode = 0x4F9302 };

	GET(HouseClass*, pHouse, ESI);

	pHouse->AISupers();

	if (pHouse->IsCurrentPlayer())
	{
		auto& sidebar = SWSidebarClass::Instance;

		for (const auto& column : sidebar.Columns)
		{
			std::vector<int> removeButtons;

			for (const auto& button : column->Buttons)
			{
				if (HouseClass::CurrentPlayer->Supers[button->SuperIndex]->IsPresent)
					continue;

				removeButtons.push_back(button->SuperIndex);
			}

			// A better solution is to swap the order of each function:
			// first check the availability of SW,
			// then remove the cameo from the SWsidebar,
			// and finally remove the cameo from the YRsidebar
			if (removeButtons.size())
				pHouse->RecheckTechTree = true;

			for (const auto& index : removeButtons)
				column->RemoveButton(index);
		}

		SWSidebarClass::Instance.SortButtons();
		int removes = 0;

		for (const auto& column : sidebar.Columns)
		{
			if (column->Buttons.empty())
				++removes;
		}

		for (; removes > 0; --removes)
			sidebar.RemoveColumn();

		if (const auto toggleButton = sidebar.ToggleButton)
			toggleButton->UpdatePosition();
	}

	return SkipGameCode;
}

DEFINE_HOOK(0x6A6316, SidebarClass_AddCameo_SuperWeapon_SWSidebar, 0x6)
{
	enum { ReturnFalse = 0x6A65FF };

	GET_STACK(AbstractType, whatAmI, STACK_OFFSET(0x14, 0x4));

	if (whatAmI != AbstractType::Special && whatAmI != AbstractType::SuperWeaponType && whatAmI != AbstractType::Super)
		return 0;

	GET_STACK(int, index, STACK_OFFSET(0x14, 0x8));

	if (SWSidebarClass::Instance.AddButton(index))
		return ReturnFalse;

	return 0;
}

DEFINE_HOOK(0x6AA790, StripClass_RecheckCameo_RemoveCameo, 0x6)
{
	enum { ShouldRemove = 0x6AA7B6, ShouldNotRemove = 0x6AAA68 };

	GET(BuildType*, pItem, ESI);
	const auto pCurrent = HouseClass::CurrentPlayer();
	const auto& supers = pCurrent->Supers;

	if (supers.ValidIndex(pItem->ItemIndex) && supers[pItem->ItemIndex]->IsPresent && !SWSidebarClass::Instance.AddButton(pItem->ItemIndex))
		return ShouldNotRemove;

	return ShouldRemove;
}

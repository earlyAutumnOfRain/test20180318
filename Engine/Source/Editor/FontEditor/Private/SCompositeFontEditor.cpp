// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "FontEditorModule.h"
#include "SCompositeFontEditor.h"
#include "SFilePathPicker.h"
#include "SInlineEditableTextBlock.h"
#include "SNumericEntryBox.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "FontEditor"

/** Entry used to weakly reference a particular typeface entry in the SListView */
struct FTypefaceListViewEntry
{
	TAttribute<FTypeface*> Typeface;
	int32 TypefaceEntryIndex;

	FTypefaceListViewEntry()
		: Typeface()
		, TypefaceEntryIndex(INDEX_NONE)
	{
	}

	FTypefaceListViewEntry(TAttribute<FTypeface*>& InTypeface, const int32 InTypefaceEntryIndex)
		: Typeface(InTypeface)
		, TypefaceEntryIndex(InTypefaceEntryIndex)
	{
	}

	void Reset()
	{
		*this = FTypefaceListViewEntry();
	}

	FTypefaceEntry* GetTypefaceEntry() const
	{
		FTypeface* const TypefacePtr = Typeface.Get(nullptr);
		return (TypefacePtr && TypefaceEntryIndex < TypefacePtr->Fonts.Num()) ? &TypefacePtr->Fonts[TypefaceEntryIndex] : nullptr;
	}
};

/** Entry used to weakly reference a particular sub-typeface entry in the SListView */
struct FSubTypefaceListViewEntry
{
	FCompositeFont* CompositeFont;
	int32 SubTypefaceEntryIndex;

	FSubTypefaceListViewEntry()
		: CompositeFont(nullptr)
		, SubTypefaceEntryIndex(INDEX_NONE)
	{
	}

	FSubTypefaceListViewEntry(FCompositeFont* const InCompositeFont, const int32 InSubTypefaceEntryIndex)
		: CompositeFont(InCompositeFont)
		, SubTypefaceEntryIndex(InSubTypefaceEntryIndex)
	{
	}

	void Reset()
	{
		*this = FSubTypefaceListViewEntry();
	}

	FCompositeSubFont* GetSubTypefaceEntry() const
	{
		return (CompositeFont && SubTypefaceEntryIndex < CompositeFont->SubTypefaces.Num()) ? &CompositeFont->SubTypefaces[SubTypefaceEntryIndex] : nullptr;
	}
};

/** Entry used to weakly reference a particular character range entry in the STileView */
struct FCharacterRangeTileViewEntry
{
	FSubTypefaceListViewEntryPtr SubTypefaceEntry;
	int32 RangeEntryIndex;

	FCharacterRangeTileViewEntry()
		: SubTypefaceEntry()
		, RangeEntryIndex(INDEX_NONE)
	{
	}

	FCharacterRangeTileViewEntry(FSubTypefaceListViewEntryPtr InSubTypefaceEntry, const int32 InRangeEntryIndex)
		: SubTypefaceEntry(InSubTypefaceEntry)
		, RangeEntryIndex(InRangeEntryIndex)
	{
	}

	void Reset()
	{
		*this = FCharacterRangeTileViewEntry();
	}

	FInt32Range* GetRange() const
	{
		FCompositeSubFont* const SubTypefaceEntryPtr = (SubTypefaceEntry.IsValid()) ? SubTypefaceEntry->GetSubTypefaceEntry() : nullptr;
		return (SubTypefaceEntryPtr && RangeEntryIndex < SubTypefaceEntryPtr->CharacterRanges.Num()) ? &SubTypefaceEntryPtr->CharacterRanges[RangeEntryIndex] : nullptr;
	}
};

SCompositeFontEditor::~SCompositeFontEditor()
{
}

void SCompositeFontEditor::Construct(const FArguments& InArgs)
{
	FontEditorPtr = InArgs._FontEditor;

	ChildSlot
	[
		SNew(SScrollBox)

		+SScrollBox::Slot()
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(DefaultTypefaceEditor, STypefaceEditor)
				.CompositeFontEditor(this)
				.Typeface(this, &SCompositeFontEditor::GetDefaultTypeface)
				.TypefaceDisplayName(LOCTEXT("DefaultFontFamilyName", "Default Font Family"))
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(SubTypefaceEntriesListView, SListView<FSubTypefaceListViewEntryPtr>)
				.ListItemsSource(&SubTypefaceEntries)
				.SelectionMode(ESelectionMode::None)
				.OnGenerateRow(this, &SCompositeFontEditor::MakeSubTypefaceEntryWidget)
			]
		]
	];

	UpdateSubTypefaceList();
}

void SCompositeFontEditor::Refresh()
{
	FlushCachedFont();

	DefaultTypefaceEditor->Refresh();
	UpdateSubTypefaceList();
}

void SCompositeFontEditor::FlushCachedFont()
{
	FCompositeFont* const CompositeFont = GetCompositeFont();
	if(CompositeFont)
	{
		CompositeFont->MakeDirty();
	}

	TSharedPtr<IFontEditor> FontEditor = FontEditorPtr.Pin();
	if(FontEditor.IsValid())
	{
		FontEditor->RefreshPreview();
	}
}

UFont* SCompositeFontEditor::GetFontObject() const
{
	TSharedPtr<IFontEditor> FontEditor = FontEditorPtr.Pin();
	return (FontEditor.IsValid()) ? FontEditor->GetFont() : nullptr;
}

FCompositeFont* SCompositeFontEditor::GetCompositeFont() const
{
	UFont* const FontObject = GetFontObject();
	return (FontObject) ? &FontObject->CompositeFont : nullptr;
}

FTypeface* SCompositeFontEditor::GetDefaultTypeface() const
{
	UFont* const FontObject = GetFontObject();
	return (FontObject) ? &FontObject->CompositeFont.DefaultTypeface : nullptr;
}

const FTypeface* SCompositeFontEditor::GetConstDefaultTypeface() const
{
	return GetDefaultTypeface();
}

void SCompositeFontEditor::UpdateSubTypefaceList()
{
	for(FSubTypefaceListViewEntryPtr& SubTypefaceListViewEntry : SubTypefaceEntries)
	{
		SubTypefaceListViewEntry->Reset();
	}

	FCompositeFont* const CompositeFontPtr = GetCompositeFont();
	if(CompositeFontPtr)
	{
		SubTypefaceEntries.Empty(CompositeFontPtr->SubTypefaces.Num());

		for(int32 SubTypefaceEntryIndex = 0; SubTypefaceEntryIndex < CompositeFontPtr->SubTypefaces.Num(); ++SubTypefaceEntryIndex)
		{
			SubTypefaceEntries.Add(MakeShareable(new FSubTypefaceListViewEntry(CompositeFontPtr, SubTypefaceEntryIndex)));
		}
	}

	// Add a dummy entry for the "Add" button slot
	SubTypefaceEntries.Add(MakeShareable(new FSubTypefaceListViewEntry()));

	SubTypefaceEntriesListView->RequestListRefresh();
}

TSharedRef<ITableRow> SCompositeFontEditor::MakeSubTypefaceEntryWidget(FSubTypefaceListViewEntryPtr InSubTypefaceEntry, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<SWidget> EntryWidget;

	if(InSubTypefaceEntry->SubTypefaceEntryIndex == INDEX_NONE)
	{
		// Dummy entry for the "Add" button
		EntryWidget = SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		[
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.ForegroundColor(FSlateColor::UseForeground())
			.ToolTipText(LOCTEXT("AddSubFontFamilyTooltip", "Add a sub-font family to this composite font"))
			.OnClicked(this, &SCompositeFontEditor::OnAddSubFontFamily)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FEditorStyle::Get().GetBrush("FontEditor.Button_Add"))
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(LOCTEXT("AddSubFontFamily", "Add Sub-Font Family"))
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
					.Justification(ETextJustify::Center)
				]
			]
		];
	}
	else
	{
		const int32 SubFontIndex = SubTypefaceEntries.IndexOfByKey(InSubTypefaceEntry);
		const FText SubFontFamilyName = FText::Format(LOCTEXT("SubFontFamilyNameFmt", "Sub-Font Family #{0}"), FText::AsNumber(SubFontIndex + 1));

		EntryWidget = SNew(SSubTypefaceEditor)
		.CompositeFontEditor(this)
		.SubTypeface(InSubTypefaceEntry)
		.ParentTypeface(this, &SCompositeFontEditor::GetConstDefaultTypeface)
		.OnDeleteSubFontFamily(this, &SCompositeFontEditor::OnDeleteSubFontFamily)
		.TypefaceDisplayName(SubFontFamilyName);
	}

	return
		SNew(STableRow<FSubTypefaceListViewEntryPtr>, OwnerTable)
		[
			EntryWidget.ToSharedRef()
		];
}

FReply SCompositeFontEditor::OnAddSubFontFamily()
{
	const FScopedTransaction Transaction(LOCTEXT("AddSubFontFamily", "Add Sub-Font Family"));
	GetFontObject()->Modify();

	FCompositeFont* const CompositeFontPtr = GetCompositeFont();
	if(CompositeFontPtr)
	{
		CompositeFontPtr->SubTypefaces.Add(FCompositeSubFont());
		UpdateSubTypefaceList();

		FlushCachedFont();
	}

	return FReply::Handled();
}

void SCompositeFontEditor::OnDeleteSubFontFamily(const FSubTypefaceListViewEntryPtr& SubTypefaceEntryToRemove)
{
	const FScopedTransaction Transaction(LOCTEXT("DeleteSubFontFamily", "Delete Sub-Font Family"));
	GetFontObject()->Modify();

	FCompositeFont* const CompositeFontPtr = GetCompositeFont();
	if(CompositeFontPtr)
	{
		CompositeFontPtr->SubTypefaces.RemoveAt(SubTypefaceEntryToRemove->SubTypefaceEntryIndex);
		UpdateSubTypefaceList();

		FlushCachedFont();
	}
}

STypefaceEditor::~STypefaceEditor()
{
}

void STypefaceEditor::Construct(const FArguments& InArgs)
{
	CompositeFontEditorPtr = InArgs._CompositeFontEditor;
	Typeface = InArgs._Typeface;

	ChildSlot
	.Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f))
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 8.0f, 16.0f, 8.0f))
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				[
					SNew(STextBlock)
					.Text(InArgs._TypefaceDisplayName)
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					InArgs._HeaderContent.Widget
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				InArgs._BodyContent.Widget
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(TypefaceEntriesListView, SListView<FTypefaceListViewEntryPtr>)
				.ListItemsSource(&TypefaceEntries)
				.SelectionMode(ESelectionMode::None)
				.OnGenerateRow(this, &STypefaceEditor::MakeTypefaceEntryWidget)
			]
		]
	];

	UpdateFontList();
}

void STypefaceEditor::Refresh()
{
	UpdateFontList();
}

void STypefaceEditor::UpdateFontList()
{
	FTypeface* const TypefacePtr = Typeface.Get(nullptr);

	for(FTypefaceListViewEntryPtr& TypefaceListViewEntry : TypefaceEntries)
	{
		TypefaceListViewEntry->Reset();
	}

	TypefaceEntries.Empty((TypefacePtr) ? TypefacePtr->Fonts.Num() : 0);

	if(TypefacePtr)
	{
		for(int32 TypefaceEntryIndex = 0; TypefaceEntryIndex < TypefacePtr->Fonts.Num(); ++TypefaceEntryIndex)
		{
			TypefaceEntries.Add(MakeShareable(new FTypefaceListViewEntry(Typeface, TypefaceEntryIndex)));
		}
	}

	// Add a dummy entry for the "Add" button slot
	TypefaceEntries.Add(MakeShareable(new FTypefaceListViewEntry()));

	TypefaceEntriesListView->RequestListRefresh();
}

TSharedRef<ITableRow> STypefaceEditor::MakeTypefaceEntryWidget(FTypefaceListViewEntryPtr InTypefaceEntry, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<SWidget> EntryWidget;

	if(InTypefaceEntry->TypefaceEntryIndex == INDEX_NONE)
	{
		// Dummy entry for the "Add" button
		EntryWidget = SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.ForegroundColor(FSlateColor::UseForeground())
			.ToolTipText(LOCTEXT("AddFontTooltip", "Add a new font to this font family"))
			.OnClicked(this, &STypefaceEditor::OnAddFont)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FEditorStyle::Get().GetBrush("FontEditor.Button_Add"))
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(LOCTEXT("AddFont", "Add Font"))
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
					.Justification(ETextJustify::Center)
				]
			]
		];
	}
	else
	{
		EntryWidget = SNew(STypefaceEntryEditor)
		.CompositeFontEditor(CompositeFontEditorPtr)
		.TypefaceEntry(InTypefaceEntry)
		.OnDeleteFont(this, &STypefaceEditor::OnDeleteFont)
		.OnVerifyFontName(this, &STypefaceEditor::OnVerifyFontName);
	}

	return
		SNew(STableRow<FTypefaceListViewEntryPtr>, OwnerTable)
		[
			SNew(SBox)
			.Padding(FMargin(8.0f, 0.0f, 8.0f, 8.0f))
			[
				EntryWidget.ToSharedRef()
			]
		];
}

FReply STypefaceEditor::OnAddFont()
{
	FTypeface* const TypefacePtr = Typeface.Get(nullptr);

	if(TypefacePtr)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddFont", "Add Font"));
		CompositeFontEditorPtr->GetFontObject()->Modify();

		TSet<FName> ExistingFontNames;
		for(const FTypefaceEntry& TypefaceEntry : TypefacePtr->Fonts)
		{
			ExistingFontNames.Add(TypefaceEntry.Name);
		}

		// Get a valid default name for the font
		static const FName BaseFontName("Font");
		FName NewFontName = BaseFontName;
		while(ExistingFontNames.Contains(NewFontName))
		{
			NewFontName.SetNumber(NewFontName.GetNumber() + 1);
		}

		TypefacePtr->Fonts.Add(FTypefaceEntry(NewFontName));
		UpdateFontList();

		CompositeFontEditorPtr->FlushCachedFont();
	}

	return FReply::Handled();
}

void STypefaceEditor::OnDeleteFont(const FTypefaceListViewEntryPtr& TypefaceEntryToRemove)
{
	FTypeface* const TypefacePtr = Typeface.Get(nullptr);

	if(TypefacePtr && TypefaceEntryToRemove.IsValid() && TypefaceEntryToRemove->GetTypefaceEntry())
	{
		const FScopedTransaction Transaction(LOCTEXT("DeleteFont", "Delete Font"));
		CompositeFontEditorPtr->GetFontObject()->Modify();

		TypefacePtr->Fonts.RemoveAt(TypefaceEntryToRemove->TypefaceEntryIndex);
		UpdateFontList();

		CompositeFontEditorPtr->FlushCachedFont();
	}
}

bool STypefaceEditor::OnVerifyFontName(const FTypefaceListViewEntryPtr& TypefaceEntryBeingRenamed, const FName& NewName, FText& OutFailureReason) const
{
	FTypeface* const TypefacePtr = Typeface.Get(nullptr);
	FTypefaceEntry* const TypefaceEntryPtr = TypefaceEntryBeingRenamed->GetTypefaceEntry();

	// Empty names are invalid
	if(NewName.IsNone())
	{
		OutFailureReason = LOCTEXT("Error_FontNameEmpty", "The font name cannot be empty or 'None'");
		return false;
	}

	// If we already have this name, it's valid
	if(TypefaceEntryPtr && TypefaceEntryPtr->Name == NewName)
	{
		return true;
	}

	// Duplicate names are invalid
	if(TypefacePtr)
	{
		const bool bNameExists = TypefacePtr->Fonts.ContainsByPredicate([&NewName](const FTypefaceEntry& TypefaceEntry) -> bool
		{
			return TypefaceEntry.Name == NewName;
		});

		if(bNameExists)
		{
			OutFailureReason = FText::Format(LOCTEXT("Error_DuplicateFontNameFmt", "A font with the name '{0}' already exists"), FText::FromName(NewName));
			return false;
		}
	}

	return true;
}

STypefaceEntryEditor::~STypefaceEntryEditor()
{
}

void STypefaceEntryEditor::Construct(const FArguments& InArgs)
{
	CompositeFontEditorPtr = InArgs._CompositeFontEditor;
	TypefaceEntry = InArgs._TypefaceEntry;
	OnDeleteFont = InArgs._OnDeleteFont;
	OnVerifyFontName = InArgs._OnVerifyFontName;

	GatherHintingEnumEntries();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(8.0f)
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f))
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					[
						SNew(SInlineEditableTextBlock)
						.Text(this, &STypefaceEntryEditor::GetTypefaceEntryName)
						.ToolTipText(LOCTEXT("FontNameTooltip", "The name of this font within the font family (click to edit)"))
						.OnTextCommitted(this, &STypefaceEntryEditor::OnTypefaceEntryNameCommitted)
						.OnVerifyTextChanged(this, &STypefaceEntryEditor::OnTypefaceEntryChanged)
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.Padding(2.0f)
						[
							SAssignNew(PreviewTextBlock, STextBlock)
							.Text(FText::FromString(TEXT("Preview")))
							.ToolTipText(LOCTEXT("PreviewFontTooltip", "Preview of how this font will look when rendered by Slate"))
							.Font(this, &STypefaceEntryEditor::GetPreviewFontStyle)
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					[
						SNew(SFilePathPicker)
						.BrowseButtonImage(FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
						.BrowseButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
						.BrowseButtonToolTip(LOCTEXT("FontFilePathPickerToolTip", "Choose a font file from this computer"))
						.BrowseDirectory_Static([]() { return FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN); })
						.BrowseTitle(LOCTEXT("FontPickerTitle", "Choose a font file..."))
						.FilePath(this, &STypefaceEntryEditor::GetTypefaceEntryFontFilePath)
						.FileTypeFilter(TEXT("TrueType fonts (*.ttf)|*.ttf|OpenType fonts (*.otf)|*.otf"))
						.OnPathPicked(this, &STypefaceEntryEditor::OnTypefaceEntryFontPathPicked)
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
					[
						SNew(SBox)
						.MinDesiredWidth(100.0f)
						[
							SNew(SComboBox<TSharedPtr<FFontHintingComboEntry>>)
							.OptionsSource(&HintingComboData)
							.OnSelectionChanged(this, &STypefaceEntryEditor::OnHintingComboSelectionChanged)
							.OnGenerateWidget(this, &STypefaceEntryEditor::MakeHintingComboEntryWidget)
							[
								SNew(STextBlock)
								.Text(this, &STypefaceEntryEditor::GetHintingComboText)
							]
						]
					]
				]
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.ToolTipText(LOCTEXT("DeleteFontTooltip", "Remove this font from the font family"))
				.OnClicked(this, &STypefaceEntryEditor::OnDeleteFontClicked)
				[
					SNew(SImage)
					.Image(FEditorStyle::Get().GetBrush("FontEditor.Button_Delete"))
				]
			]
		]
	];
}

FText STypefaceEntryEditor::GetTypefaceEntryName() const
{
	FTypefaceEntry* const TypefaceEntryPtr = TypefaceEntry->GetTypefaceEntry();

	if(TypefaceEntryPtr)
	{
		return FText::FromName(TypefaceEntryPtr->Name);
	}

	return FText::GetEmpty();
}

void STypefaceEntryEditor::OnTypefaceEntryNameCommitted(const FText& InNewName, ETextCommit::Type InCommitType)
{
	FTypefaceEntry* const TypefaceEntryPtr = TypefaceEntry->GetTypefaceEntry();

	if(TypefaceEntryPtr)
	{
		const FScopedTransaction Transaction(LOCTEXT("RenameFont", "Rename Font"));
		CompositeFontEditorPtr->GetFontObject()->Modify();

		TypefaceEntryPtr->Name = *InNewName.ToString();

		CompositeFontEditorPtr->FlushCachedFont();
	}
}

bool STypefaceEntryEditor::OnTypefaceEntryChanged(const FText& InNewName, FText& OutFailureReason) const
{
	return !OnVerifyFontName.IsBound() || OnVerifyFontName.Execute(TypefaceEntry, *InNewName.ToString(), OutFailureReason);
}

FString STypefaceEntryEditor::GetTypefaceEntryFontFilePath() const
{
	FTypefaceEntry* const TypefaceEntryPtr = TypefaceEntry->GetTypefaceEntry();

	if(TypefaceEntryPtr)
	{
		return TypefaceEntryPtr->Font.FontFilename;
	}

	return FString();
}

void STypefaceEntryEditor::OnTypefaceEntryFontPathPicked(const FString& InNewFontFilename)
{
	FTypefaceEntry* const TypefaceEntryPtr = TypefaceEntry->GetTypefaceEntry();

	if(TypefaceEntryPtr)
	{
		const FScopedTransaction Transaction(LOCTEXT("SetFontFile", "Set Font File"));
		CompositeFontEditorPtr->GetFontObject()->Modify();

		if(TypefaceEntryPtr->Font.SetFont(InNewFontFilename))
		{
			CompositeFontEditorPtr->FlushCachedFont();
		}
	}

	FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_OPEN, FPaths::GetPath(InNewFontFilename));
}

FReply STypefaceEntryEditor::OnDeleteFontClicked()
{
	OnDeleteFont.ExecuteIfBound(TypefaceEntry);

	return FReply::Handled();
}

FSlateFontInfo STypefaceEntryEditor::GetPreviewFontStyle() const
{
	FTypefaceEntry* const TypefaceEntryPtr = TypefaceEntry->GetTypefaceEntry();
	return FSlateFontInfo(CompositeFontEditorPtr->GetFontObject(), 9, (TypefaceEntryPtr) ? TypefaceEntryPtr->Name : NAME_None);
}

void STypefaceEntryEditor::GatherHintingEnumEntries()
{
	FTypefaceEntry* const TypefaceEntryPtr = TypefaceEntry->GetTypefaceEntry();

	const UEnum* const HintingEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EFontHinting"), true);
	check(HintingEnum);

	for(int32 EnumIndex = 0; EnumIndex < HintingEnum->NumEnums() - 1; ++EnumIndex)
	{
		// Ignore hidden enum entries
		const bool bShouldBeHidden = HintingEnum->HasMetaData(TEXT("Hidden"), EnumIndex) || HintingEnum->HasMetaData(TEXT("Spacer"), EnumIndex);
		if(bShouldBeHidden)
		{
			continue;
		}

		TSharedPtr<FFontHintingComboEntry> ComboEntry = MakeShareable(new FFontHintingComboEntry());
		ComboEntry->EnumValue = static_cast<EFontHinting>(EnumIndex);

		// See if we specified an alternate name for this entry using metadata
		ComboEntry->DisplayName = HintingEnum->GetDisplayNameText(EnumIndex);
		if(ComboEntry->DisplayName.IsEmpty()) 
		{
			ComboEntry->DisplayName = HintingEnum->GetEnumText(EnumIndex);
		}

		ComboEntry->Tooltip = HintingEnum->GetToolTipText(EnumIndex);

		if(TypefaceEntryPtr && ComboEntry->EnumValue == TypefaceEntryPtr->Font.Hinting)
		{
			ActiveHintingEnumEntryText = ComboEntry->DisplayName;
		}

		HintingComboData.Add(ComboEntry);
	}
}

void STypefaceEntryEditor::OnHintingComboSelectionChanged(TSharedPtr<FFontHintingComboEntry> InNewSelection, ESelectInfo::Type)
{
	FTypefaceEntry* const TypefaceEntryPtr = TypefaceEntry->GetTypefaceEntry();

	if(TypefaceEntryPtr && InNewSelection.IsValid() && TypefaceEntryPtr->Font.Hinting != InNewSelection->EnumValue)
	{
		const FScopedTransaction Transaction(LOCTEXT("SetFontHinting", "Set Font Hinting"));
		CompositeFontEditorPtr->GetFontObject()->Modify();

		TypefaceEntryPtr->Font.Hinting = InNewSelection->EnumValue;
		ActiveHintingEnumEntryText = InNewSelection->DisplayName;

		CompositeFontEditorPtr->FlushCachedFont();
	}
}

TSharedRef<SWidget> STypefaceEntryEditor::MakeHintingComboEntryWidget(TSharedPtr<FFontHintingComboEntry> InHintingEntry)
{
	return
		SNew(STextBlock)
		.Text(InHintingEntry->DisplayName)
		.ToolTipText(InHintingEntry->Tooltip);
}

FText STypefaceEntryEditor::GetHintingComboText() const
{
	return ActiveHintingEnumEntryText;
}

SSubTypefaceEditor::~SSubTypefaceEditor()
{
}

void SSubTypefaceEditor::Construct(const FArguments& InArgs)
{
	CompositeFontEditorPtr = InArgs._CompositeFontEditor;
	SubTypeface = InArgs._SubTypeface;
	ParentTypeface = InArgs._ParentTypeface;
	OnDeleteSubFontFamily = InArgs._OnDeleteSubFontFamily;

	ChildSlot
	[
		SAssignNew(TypefaceEditor, STypefaceEditor)
		.CompositeFontEditor(InArgs._CompositeFontEditor)
		.Typeface(this, &SSubTypefaceEditor::GetTypeface)
		.TypefaceDisplayName(InArgs._TypefaceDisplayName)
		.HeaderContent()
		[
			SNew(SBox)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(4.0f, 0.0f))
				[
					SNew(SNumericEntryBox<float>)
					.ToolTipText(LOCTEXT("ScalingFactorTooltip", "The scaling factor will adjust the size of the rendered glyphs so that you can tweak their size to match that of the default font family"))
					.Value(this, &SSubTypefaceEditor::GetScalingFactorAsOptional)
					.OnValueCommitted(this, &SSubTypefaceEditor::OnScalingFactorCommittedAsNumeric)
					.LabelVAlign(VAlign_Center)
					.Label()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ScalingFactorLabel", "Scaling Factor"))
					]
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(FontOverrideCombo, SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&FontOverrideComboData)
					.ContentPadding(FMargin(4.0, 2.0))
					.Visibility(this, &SSubTypefaceEditor::GetAddFontOverrideVisibility)
					.OnComboBoxOpening(this, &SSubTypefaceEditor::OnAddFontOverrideComboOpening)
					.OnSelectionChanged(this, &SSubTypefaceEditor::OnAddFontOverrideSelectionChanged)
					.OnGenerateWidget(this, &SSubTypefaceEditor::MakeAddFontOverrideWidget)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AddFontOverride", "Add Font Override"))
						.ToolTipText(LOCTEXT("AddFontOverrideTooltip", "Override a font from the default font family to ensure it will be used when drawing a glyph in the range of this sub-font family"))
					]
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
					.ToolTipText(LOCTEXT("DeleteFontTooltip", "Remove this sub-font family from the composite font"))
					.OnClicked(this, &SSubTypefaceEditor::OnDeleteSubFontFamilyClicked)
					[
						SNew(SImage)
						.Image(FEditorStyle::Get().GetBrush("FontEditor.Button_Delete"))
					]
				]
			]
		]
		.BodyContent()
		[
			SNew(SBox)
			.Padding(FMargin(8.0f, 0.0f, 8.0f, 0.0f))
			[
				SAssignNew(CharacterRangeEntriesTileView, STileView<FCharacterRangeTileViewEntryPtr>)
				.ListItemsSource(&CharacterRangeEntries)
				.SelectionMode(ESelectionMode::None)
				.ItemWidth(160)
				.ItemHeight(120)
				.ItemAlignment(EListItemAlignment::LeftAligned)
				.OnGenerateTile(this, &SSubTypefaceEditor::MakeCharacterRangesEntryWidget)
			]
		]
	];

	UpdateCharacterRangesList();
}

FTypeface* SSubTypefaceEditor::GetTypeface() const
{
	FCompositeSubFont* const SubTypefaceEntryPtr = SubTypeface->GetSubTypefaceEntry();
	return (SubTypefaceEntryPtr) ? &SubTypefaceEntryPtr->Typeface : nullptr;
}

EVisibility SSubTypefaceEditor::GetAddFontOverrideVisibility() const
{
	return (ParentTypeface.Get(nullptr)) ? EVisibility::Visible : EVisibility::Collapsed;
}

void SSubTypefaceEditor::OnAddFontOverrideComboOpening()
{
	FontOverrideComboData.Empty();
	
	FCompositeSubFont* const SubTypefaceEntryPtr = SubTypeface->GetSubTypefaceEntry();
	const FTypeface* const ParentTypefacePtr = ParentTypeface.Get(nullptr);

	if(SubTypefaceEntryPtr && ParentTypefacePtr)
	{
		TSet<FName> LocalFontNames;
		for(const FTypefaceEntry& LocalTypefaceEntry : SubTypefaceEntryPtr->Typeface.Fonts)
		{
			LocalFontNames.Add(LocalTypefaceEntry.Name);
		}

		// Add every font from our parent font that hasn't already got a local entry
		for(const FTypefaceEntry& ParentTypefaceEntry : ParentTypefacePtr->Fonts)
		{
			if(!LocalFontNames.Contains(ParentTypefaceEntry.Name))
			{
				FontOverrideComboData.Add(MakeShareable(new FName(ParentTypefaceEntry.Name)));
			}
		}
	}

	FontOverrideCombo->RefreshOptions();
}

void SSubTypefaceEditor::OnAddFontOverrideSelectionChanged(TSharedPtr<FName> InNewSelection, ESelectInfo::Type)
{
	FCompositeSubFont* const SubTypefaceEntryPtr = SubTypeface->GetSubTypefaceEntry();

	if(SubTypefaceEntryPtr && InNewSelection.IsValid() && !InNewSelection->IsNone())
	{
		const FScopedTransaction Transaction(LOCTEXT("AddFontOverride", "Add Font Override"));
		CompositeFontEditorPtr->GetFontObject()->Modify();

		SubTypefaceEntryPtr->Typeface.Fonts.Add(FTypefaceEntry(*InNewSelection));
		TypefaceEditor->Refresh();

		CompositeFontEditorPtr->FlushCachedFont();
	}
}

TSharedRef<SWidget> SSubTypefaceEditor::MakeAddFontOverrideWidget(TSharedPtr<FName> InFontEntry)
{
	return
		SNew(STextBlock)
		.Text(FText::FromName(*InFontEntry));
}

FReply SSubTypefaceEditor::OnDeleteSubFontFamilyClicked()
{
	OnDeleteSubFontFamily.ExecuteIfBound(SubTypeface);

	return FReply::Handled();
}

void SSubTypefaceEditor::UpdateCharacterRangesList()
{
	FCompositeSubFont* const SubTypefaceEntryPtr = SubTypeface->GetSubTypefaceEntry();

	for(FCharacterRangeTileViewEntryPtr& CharacterRangeTileViewEntry : CharacterRangeEntries)
	{
		CharacterRangeTileViewEntry->Reset();
	}

	CharacterRangeEntries.Empty((SubTypefaceEntryPtr) ? SubTypefaceEntryPtr->CharacterRanges.Num() : 0);

	if(SubTypefaceEntryPtr)
	{
		for(int32 CharacterRangeIndex = 0; CharacterRangeIndex < SubTypefaceEntryPtr->CharacterRanges.Num(); ++CharacterRangeIndex)
		{
			CharacterRangeEntries.Add(MakeShareable(new FCharacterRangeTileViewEntry(SubTypeface, CharacterRangeIndex)));
		}
	}

	// Add a dummy entry for the "Add" button slot
	CharacterRangeEntries.Add(MakeShareable(new FCharacterRangeTileViewEntry()));

	CharacterRangeEntriesTileView->RequestListRefresh();
}

TSharedRef<ITableRow> SSubTypefaceEditor::MakeCharacterRangesEntryWidget(FCharacterRangeTileViewEntryPtr InCharacterRangeEntry, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<SWidget> EntryWidget;

	if(InCharacterRangeEntry->RangeEntryIndex == INDEX_NONE)
	{
		// Dummy entry for the "Add" button
		EntryWidget = SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.ForegroundColor(FSlateColor::UseForeground())
			.ToolTipText(LOCTEXT("AddCharacterRangeTooltip", "Add a new character range to this sub-font family"))
			.OnClicked(this, &SSubTypefaceEditor::OnAddCharacterRangeClicked)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(16.0f)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image(FEditorStyle::Get().GetBrush("FontEditor.Button_Add"))
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(LOCTEXT("AddCharacterRange", "Add Character Range"))
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
					.Justification(ETextJustify::Center)
				]
			]
		];
	}
	else
	{
		EntryWidget = SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SCharacterRangeEditor)
				.CompositeFontEditor(CompositeFontEditorPtr)
				.CharacterRange(InCharacterRangeEntry)
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.ToolTipText(LOCTEXT("DeleteCharacterRangeTooltip", "Remove this character range from the sub-font family"))
				.OnClicked(this, &SSubTypefaceEditor::OnDeleteCharacterRangeClicked, InCharacterRangeEntry)
				[
					SNew(SImage)
					.Image(FEditorStyle::Get().GetBrush("FontEditor.Button_Delete"))
				]
			]
		];
	}

	return
		SNew(STableRow<FCharacterRangeTileViewEntryPtr>, OwnerTable)
		[
			SNew(SBox)
			.Padding(FMargin(0.0f, 0.0f, 8.0f, 8.0f))
			[
				EntryWidget.ToSharedRef()
			]
		];
}

FReply SSubTypefaceEditor::OnAddCharacterRangeClicked()
{
	FCompositeSubFont* const SubTypefaceEntryPtr = SubTypeface->GetSubTypefaceEntry();

	if(SubTypefaceEntryPtr)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddCharacterRange", "Add Character Range"));
		CompositeFontEditorPtr->GetFontObject()->Modify();

		SubTypefaceEntryPtr->CharacterRanges.Add(FInt32Range::Empty());

		UpdateCharacterRangesList();

		CompositeFontEditorPtr->FlushCachedFont();
	}

	return FReply::Handled();
}

FReply SSubTypefaceEditor::OnDeleteCharacterRangeClicked(FCharacterRangeTileViewEntryPtr InCharacterRangeEntry)
{
	FCompositeSubFont* const SubTypefaceEntryPtr = SubTypeface->GetSubTypefaceEntry();

	if(SubTypefaceEntryPtr)
	{
		const FScopedTransaction Transaction(LOCTEXT("DeleteCharacterRange", "Delete Character Range"));
		CompositeFontEditorPtr->GetFontObject()->Modify();

		SubTypefaceEntryPtr->CharacterRanges.RemoveAt(InCharacterRangeEntry->RangeEntryIndex);

		UpdateCharacterRangesList();

		CompositeFontEditorPtr->FlushCachedFont();
	}

	return FReply::Handled();
}

TOptional<float> SSubTypefaceEditor::GetScalingFactorAsOptional() const
{
	const FCompositeSubFont* const SubTypefaceEntryPtr = SubTypeface->GetSubTypefaceEntry();

	if(SubTypefaceEntryPtr)
	{
		return SubTypefaceEntryPtr->ScalingFactor;
	}

	return TOptional<float>();
}

void SSubTypefaceEditor::OnScalingFactorCommittedAsNumeric(float InNewValue, ETextCommit::Type InCommitType)
{
	FCompositeSubFont* const SubTypefaceEntryPtr = SubTypeface->GetSubTypefaceEntry();

	if(SubTypefaceEntryPtr)
	{
		const FScopedTransaction Transaction(LOCTEXT("SetScalingFactor", "Set Scaling Factor"));
		CompositeFontEditorPtr->GetFontObject()->Modify();

		SubTypefaceEntryPtr->ScalingFactor = InNewValue;

		CompositeFontEditorPtr->FlushCachedFont();
	}
}

SCharacterRangeEditor::~SCharacterRangeEditor()
{
}

void SCharacterRangeEditor::Construct(const FArguments& InArgs)
{
	CompositeFontEditorPtr = InArgs._CompositeFontEditor;
	CharacterRange = InArgs._CharacterRange;

	ChildSlot
	[
		SNew(SGridPanel)

		// Minimum column
		+SGridPanel::Slot(0, 0)
		.Padding(2.0f)
		[
			SNew(SEditableTextBox)
			.Text(this, &SCharacterRangeEditor::GetRangeComponentAsTCHAR, 0)
			.OnTextCommitted(this, &SCharacterRangeEditor::OnRangeComponentCommittedAsTCHAR, 0)
		]

		+SGridPanel::Slot(0, 1)
		.Padding(2.0f)
		[
			SNew(SEditableTextBox)
			.Text(this, &SCharacterRangeEditor::GetRangeComponentAsHexString, 0)
			.OnTextCommitted(this, &SCharacterRangeEditor::OnRangeComponentCommittedAsHexString, 0)
		]

		+SGridPanel::Slot(0, 2)
		.Padding(2.0f)
		[
			SNew(SNumericEntryBox<int32>)
			.Value(this, &SCharacterRangeEditor::GetRangeComponentAsOptional, 0)
			.OnValueCommitted(this, &SCharacterRangeEditor::OnRangeComponentCommittedAsNumeric, 0)
		]

		// Separator
		+SGridPanel::Slot(1, 0)
		.RowSpan(3)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT(" - ")))
			.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
		]

		// Maximum column
		+SGridPanel::Slot(2, 0)
		.Padding(2.0f)
		[
			SNew(SEditableTextBox)
			.Text(this, &SCharacterRangeEditor::GetRangeComponentAsTCHAR, 1)
			.OnTextCommitted(this, &SCharacterRangeEditor::OnRangeComponentCommittedAsTCHAR, 1)
		]

		+SGridPanel::Slot(2, 1)
		.Padding(2.0f)
		[
			SNew(SEditableTextBox)
			.Text(this, &SCharacterRangeEditor::GetRangeComponentAsHexString, 1)
			.OnTextCommitted(this, &SCharacterRangeEditor::OnRangeComponentCommittedAsHexString, 1)
		]

		+SGridPanel::Slot(2, 2)
		.Padding(2.0f)
		[
			SNew(SNumericEntryBox<int32>)
			.Value(this, &SCharacterRangeEditor::GetRangeComponentAsOptional, 1)
			.OnValueCommitted(this, &SCharacterRangeEditor::OnRangeComponentCommittedAsNumeric, 1)
		]
	];
}

FText SCharacterRangeEditor::GetRangeComponentAsTCHAR(int32 InComponentIndex) const
{
	const int32 RangeComponent = GetRangeComponent(InComponentIndex);
	const TCHAR RangeComponentStr[] = { static_cast<TCHAR>(RangeComponent), 0 };
	return FText::FromString(RangeComponentStr);
}

FText SCharacterRangeEditor::GetRangeComponentAsHexString(int32 InComponentIndex) const
{
	const int32 RangeComponent = GetRangeComponent(InComponentIndex);
	return FText::FromString(FString::Printf(TEXT("0x%04x"), RangeComponent));
}

TOptional<int32> SCharacterRangeEditor::GetRangeComponentAsOptional(int32 InComponentIndex) const
{
	return GetRangeComponent(InComponentIndex);
}

int32 SCharacterRangeEditor::GetRangeComponent(const int32 InComponentIndex) const
{
	check(InComponentIndex == 0 || InComponentIndex == 1);

	const FInt32Range* const CharacterRangePtr = CharacterRange->GetRange();
	if(CharacterRangePtr)
	{
		return (InComponentIndex == 0) ? CharacterRangePtr->GetLowerBoundValue() : CharacterRangePtr->GetUpperBoundValue();
	}

	return 0;
}

void SCharacterRangeEditor::OnRangeComponentCommittedAsTCHAR(const FText& InNewValue, ETextCommit::Type InCommitType, int32 InComponentIndex)
{
	const FString NewValueStr = InNewValue.ToString();
	if(NewValueStr.Len() == 1)
	{
		SetRangeComponent(static_cast<int32>(NewValueStr[0]), InComponentIndex);
	}
	else if(NewValueStr.Len() == 0)
	{
		SetRangeComponent(0, InComponentIndex);
	}
}

void SCharacterRangeEditor::OnRangeComponentCommittedAsHexString(const FText& InNewValue, ETextCommit::Type InCommitType, int32 InComponentIndex)
{
	const FString NewValueStr = InNewValue.ToString();
	const TCHAR* HexStart = NewValueStr.GetCharArray().GetData();
	if(NewValueStr.StartsWith(TEXT("0x")))
	{
		// Skip the "0x" part, as FParse::HexNumber doesn't handle that
		HexStart += 2;
	}

	const int32 NewValue = FParse::HexNumber(HexStart);
	SetRangeComponent(NewValue, InComponentIndex);
}

void SCharacterRangeEditor::OnRangeComponentCommittedAsNumeric(int32 InNewValue, ETextCommit::Type InCommitType, int32 InComponentIndex)
{
	SetRangeComponent(InNewValue, InComponentIndex);
}

void SCharacterRangeEditor::SetRangeComponent(const int32 InNewValue, const int32 InComponentIndex)
{
	check(InComponentIndex == 0 || InComponentIndex == 1);
	
	FInt32Range* const CharacterRangePtr = CharacterRange->GetRange();
	if(CharacterRangePtr)
	{
		const FScopedTransaction Transaction(LOCTEXT("UpdateCharacterRange", "Update Character Range"));
		CompositeFontEditorPtr->GetFontObject()->Modify();

		*CharacterRangePtr = (InComponentIndex == 0) 
			? FInt32Range(FInt32Range::BoundsType::Inclusive(InNewValue), FInt32Range::BoundsType::Inclusive(CharacterRangePtr->GetUpperBoundValue())) 
			: FInt32Range(FInt32Range::BoundsType::Inclusive(CharacterRangePtr->GetLowerBoundValue()), FInt32Range::BoundsType::Inclusive(InNewValue));

		CompositeFontEditorPtr->FlushCachedFont();
	}
}

#undef LOCTEXT_NAMESPACE

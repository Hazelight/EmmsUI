#include "EmmsAttribute.h"

TMap<FProperty*, FEmmsAttributeSpecification*> FEmmsAttributeSpecification::AttributeSpecsByProperty;
TMap<TPair<FName, UClass*>, FEmmsAttributeSpecification*> FEmmsAttributeSpecification::SlotAttributeSpecs;
TMap<FEmmsAttributeSpecification*, FEmmsAttributeSpecification*> FEmmsAttributeSpecification::SlotAttributeGenericSpecs;

FName FEmmsAttributeSpecification::GetAttributeName() const
{
	switch (Type)
	{
		case EEmmsAttributeType::Property:
		case EEmmsAttributeType::BitField:
			return AttributeProperty->GetFName();
	}

	check(false);
	return NAME_None;
}

void FEmmsAttributeSpecification::InitializeValue(FEmmsAttributeValue& Value) const
{
	switch (Type)
	{
		case EEmmsAttributeType::Property:
		{
			if (Value.Data.IsEmpty())
			{
				Value.Data.SetNumZeroed(AttributeProperty->GetElementSize());
				AttributeProperty->InitializeValue(Value.Data.GetData());
			}
		}
		break;
		case EEmmsAttributeType::BitField:
		{
			if (Value.Data.IsEmpty())
			{
				Value.Data.SetNumZeroed(1);
			}
		}
		break;
		default:
			check(false);
		break;
	}
}

void FEmmsAttributeSpecification::ResetValue(FEmmsAttributeValue& Value) const
{
	switch (Type)
	{
		case EEmmsAttributeType::Property:
		{
			if (!Value.Data.IsEmpty())
			{
				AttributeProperty->DestroyValue(Value.Data.GetData());
				Value.Data.Reset();
			}
		}
		break;
		case EEmmsAttributeType::BitField:
		{
			if (!Value.Data.IsEmpty())
				Value.Data.Reset();
		}
		break;
		default:
			check(false);
		break;
	}
}

void FEmmsAttributeSpecification::AssignValue(FEmmsAttributeValue& Value, void* DataPtr) const
{
	switch (Type)
	{
		case EEmmsAttributeType::Property:
		{
			if (Value.Data.IsEmpty())
			{
				Value.Data.SetNumZeroed(AttributeProperty->GetElementSize());
				AttributeProperty->InitializeValue(Value.Data.GetData());
			}

			AttributeProperty->CopyCompleteValue(
				Value.Data.GetData(),
				DataPtr
			);
		}
		break;
		case EEmmsAttributeType::BitField:
		{
			if (Value.Data.IsEmpty())
				Value.Data.SetNumZeroed(1);
			*(bool*)Value.GetDataPtr() = *(bool*)DataPtr;
		}
		break;
		default:
			check(false);
		break;
	}
}

bool FEmmsAttributeSpecification::IsCompatibleWithContainer(UObject* Object) const
{
	if (Object == nullptr)
		return false;

	switch (Type)
	{
		case EEmmsAttributeType::Property:
		case EEmmsAttributeType::BitField:
			return Object->IsA(AttributeProperty->GetOwnerClass());
	}

	return false;
}

void FEmmsAttributeState::SetPendingValue(FEmmsAttributeSpecification* Spec, void* Value)
{
	Spec->AssignValue(PendingValue, Value);
}

void FEmmsAttributeState::Reset(FEmmsAttributeSpecification* Spec)
{
	if (!CurrentValue.IsEmpty())
		Spec->ResetValue(CurrentValue);

	if (!PendingValue.IsEmpty())
		Spec->ResetValue(PendingValue);

	if (!DefaultValue.IsEmpty())
		Spec->ResetValue(DefaultValue);

	if (!MirroredValue.IsEmpty())
		Spec->ResetValue(MirroredValue);
}

void FEmmsAttributeState::MovePendingToNewContainer(FEmmsAttributeSpecification* Spec)
{
	if (!CurrentValue.IsEmpty())
		Spec->ResetValue(CurrentValue);

	if (!DefaultValue.IsEmpty())
		Spec->ResetValue(DefaultValue);
}

void FEmmsAttributeState::ApplyCurrentToNewContainer(FEmmsAttributeSpecification* Spec, void* Container)
{
	if (!CurrentValue.IsEmpty())
	{
		switch (Spec->Type)
		{
			case EEmmsAttributeType::Property:
			{
				if (Spec->AssignValueFunction)
					Spec->AssignValueFunction(Spec, Container, CurrentValue.GetDataPtr());
				else
					Spec->AttributeProperty->SetValue_InContainer(Container, CurrentValue.GetDataPtr());
			}
			break;
			case EEmmsAttributeType::BitField:
			{
				if (Spec->AssignValueFunction)
					Spec->AssignValueFunction(Spec, Container, CurrentValue.GetDataPtr());
				else if (Spec->AttributeProperty->HasSetter())
					Spec->AttributeProperty->CallSetter(Container, CurrentValue.GetDataPtr());
				else
					((FBoolProperty*)Spec->AttributeProperty)->SetPropertyValue_InContainer(Container, *(bool*)CurrentValue.GetDataPtr());
			}
			break;
			default: check(false); break;
		}
	}
}

bool FEmmsAttributeState::Update(FEmmsAttributeSpecification* Spec, void* Container)
{
	auto PrevValue = MoveTemp(CurrentValue);
	bool bValueWasChanged = false;

	switch (Spec->Type)
	{
		case EEmmsAttributeType::Property:
		{
			bool bShouldApply = false;
			if (PrevValue.IsEmpty())
			{
				if (!PendingValue.IsEmpty())
				{
					if (DefaultValue.IsEmpty())
						Spec->InitializeValue(DefaultValue);

					// Take the current value of the property and store it as the default value,
					// this value will be used when a frame _doesn't_ set a value for the property
					Spec->AttributeProperty->GetValue_InContainer(
						Container,
						DefaultValue.GetDataPtr()
					);

					// Set the property to the value that was specified this frame
					if (Spec->AssignValueFunction)
						Spec->AssignValueFunction(Spec, Container, PendingValue.GetDataPtr());
					else
						Spec->AttributeProperty->SetValue_InContainer(Container, PendingValue.GetDataPtr());
					bValueWasChanged = true;
				}
			}
			else
			{
				if (PendingValue.IsEmpty())
				{
					// Reset back to the default value now that we no longer have any value
					if (Spec->ResetToDefaultFunction)
					{
						Spec->ResetToDefaultFunction(Spec, Container);
						bValueWasChanged = true;
					}
					else if (!DefaultValue.IsEmpty())
					{
						if (Spec->AssignValueFunction)
							Spec->AssignValueFunction(Spec, Container, DefaultValue.GetDataPtr());
						else
							Spec->AttributeProperty->SetValue_InContainer(Container, DefaultValue.GetDataPtr());

						bValueWasChanged = true;

						// Remove the data for the default value, since we want to get a new value if we ever set it again
						Spec->ResetValue(DefaultValue);
					}
				}
				else
				{
					bool bValueIdentical = false;
					if (Spec->CompareValueFunction)
					{
						bValueIdentical = Spec->CompareValueFunction(Spec, Container, PrevValue.GetDataPtr(), PendingValue.GetDataPtr());
					}
					else
					{
						bValueIdentical = Spec->AttributeProperty->Identical(
							PrevValue.GetDataPtr(),
							PendingValue.GetDataPtr()
						);
					}

					if (!bValueIdentical)
					{
						// Set the property to the value that was specified this frame
						if (Spec->AssignValueFunction)
							Spec->AssignValueFunction(Spec, Container, PendingValue.GetDataPtr());
						else
							Spec->AttributeProperty->SetValue_InContainer(Container, PendingValue.GetDataPtr());
						bValueWasChanged = true;
					}
				}

				// Destruct old value that we're overriding
				Spec->ResetValue(PrevValue);
			}
		}
		break;
		case EEmmsAttributeType::BitField:
		{
			FBoolProperty* BoolProperty = (FBoolProperty*)Spec->AttributeProperty;

			bool bShouldApply = false;
			if (PrevValue.IsEmpty())
			{
				if (!PendingValue.IsEmpty())
				{
					if (DefaultValue.IsEmpty())
						Spec->InitializeValue(DefaultValue);

					// Take the current value of the property and store it as the default value,
					// this value will be used when a frame _doesn't_ set a value for the property
					if (BoolProperty->HasGetter())
						BoolProperty->CallGetter(Container, DefaultValue.GetDataPtr());
					else
						*(bool*)DefaultValue.GetDataPtr() = BoolProperty->GetPropertyValue_InContainer(Container);

					// Set the property to the value that was specified this frame
					if (Spec->AssignValueFunction)
						Spec->AssignValueFunction(Spec, Container, PendingValue.GetDataPtr());
					else if (BoolProperty->HasSetter())
						BoolProperty->CallSetter(Container, PendingValue.GetDataPtr());
					else
						BoolProperty->SetPropertyValue_InContainer(Container, *(bool*)PendingValue.GetDataPtr());
					bValueWasChanged = true;
				}
			}
			else
			{
				if (PendingValue.IsEmpty())
				{
					// Reset back to the default value now that we no longer have any value
					if (Spec->ResetToDefaultFunction)
					{
						Spec->ResetToDefaultFunction(Spec, Container);
						bValueWasChanged = true;
					}
					else if (!DefaultValue.IsEmpty())
					{
						if (Spec->AssignValueFunction)
							Spec->AssignValueFunction(Spec, Container, DefaultValue.GetDataPtr());
						else if (BoolProperty->HasSetter())
							BoolProperty->CallSetter(Container, DefaultValue.GetDataPtr());
						else
							BoolProperty->SetPropertyValue_InContainer(Container, *(bool*)DefaultValue.GetDataPtr());

						bValueWasChanged = true;

						// Remove the data for the default value, since we want to get a new value if we ever set it again
						Spec->ResetValue(DefaultValue);
					}
				}
				else if (*(bool*)PrevValue.GetDataPtr() != *(bool*)PendingValue.GetDataPtr())
				{
					// Set the property to the value that was specified this frame
					if (Spec->AssignValueFunction)
						Spec->AssignValueFunction(Spec, Container, PendingValue.GetDataPtr());
					else if (BoolProperty->HasSetter())
						BoolProperty->CallSetter(Container, PendingValue.GetDataPtr());
					else
						BoolProperty->SetPropertyValue_InContainer(Container, *(bool*)PendingValue.GetDataPtr());
					bValueWasChanged = true;
				}

				// Destruct old value that we're overriding
				Spec->ResetValue(PrevValue);
			}
		}
		break;
		default: check(false); break;
	}

	// Swap memory allocations for the current and pending values
	CurrentValue = MoveTemp(PendingValue);
	PendingValue = MoveTemp(PrevValue);
	return bValueWasChanged;
}

void FEmmsAttributeState::UpdateMirroredValue(FEmmsAttributeSpecification* Spec, void* Container)
{
	switch (Spec->Type)
	{
		case EEmmsAttributeType::Property:
		{
			bool bShouldApply = false;
			if (MirroredValue.IsEmpty())
				Spec->InitializeValue(MirroredValue);
			Spec->AttributeProperty->GetValue_InContainer(Container, MirroredValue.GetDataPtr());
		}
		break;
		case EEmmsAttributeType::BitField:
		{
			bool bShouldApply = false;
			if (MirroredValue.IsEmpty())
				Spec->InitializeValue(MirroredValue);

			FBoolProperty* BoolProperty = (FBoolProperty*)Spec->AttributeProperty;
			if (BoolProperty->HasGetter())
				BoolProperty->CallGetter(Container, MirroredValue.GetDataPtr());
			else
				*(bool*)MirroredValue.GetDataPtr() = BoolProperty->GetPropertyValue_InContainer(Container);
		}
		break;
		default: check(false); break;
	}
}

void FEmmsAttributeSpecification::AddReferencedObjects(FReferenceCollector& Collector, FEmmsAttributeValue& Value)
{
	if (Value.IsEmpty())
		return;

	switch (Type)
	{
		case EEmmsAttributeType::Property:
		{
			if (FStructProperty* StructProp = CastField<FStructProperty>(AttributeProperty))
			{
				Collector.AddPropertyReferencesWithStructARO(StructProp->Struct, Value.GetDataPtr());
			}
			else if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(AttributeProperty))
			{
				Collector.AddReferencedObject(*(TObjectPtr<UObject>*)Value.GetDataPtr());
			}
			else
			{
				check(false);
			}
		}
		break;
	}
}

void FEmmsAttributeState::AddReferencedObjects(FEmmsAttributeSpecification* Spec, FReferenceCollector& Collector)
{
	if (!DefaultValue.IsEmpty())
		Spec->AddReferencedObjects(Collector, DefaultValue);
	if (!CurrentValue.IsEmpty())
		Spec->AddReferencedObjects(Collector, CurrentValue);
	if (!PendingValue.IsEmpty())
		Spec->AddReferencedObjects(Collector, PendingValue);
	if (!MirroredValue.IsEmpty())
		Spec->AddReferencedObjects(Collector, MirroredValue);
}
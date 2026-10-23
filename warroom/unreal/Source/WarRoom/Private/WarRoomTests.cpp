// Unreal automation tests around the simulation: the same facts the standalone
// Tests/sim_tests.cpp proves, runnable from the editor's Session Frontend.
#include "Misc/AutomationTest.h"
#include "WarRoomSubsystem.h"
#include "WarRoomSim/WarRoomSim.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarRoomClockTest, "WarRoom.Sim.Clock", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FWarRoomClockTest::RunTest(const FString& Parameters)
{
	const warroom::Date D = warroom::HourToDate(0);
	TestEqual(TEXT("year"), D.year, 1936); TestEqual(TEXT("month"), D.month, 1); TestEqual(TEXT("day"), D.day, 1);
	TestEqual(TEXT("format"), FString(UTF8_TO_TCHAR(warroom::FormatDate(24 * 31).c_str())), FString(TEXT("1 Feb 1936, 00:00")));
	TestTrue(TEXT("leap"), warroom::IsLeapYear(1936));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarRoomDataTest, "WarRoom.Sim.RealData", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FWarRoomDataTest::RunTest(const FString& Parameters)
{
	warroom::Data Data;
	const std::string Err = warroom::LoadData(TCHAR_TO_UTF8(*UWarRoomSubsystem::FindDataDir()), Data);
	TestTrue(FString::Printf(TEXT("data loads: %s"), UTF8_TO_TCHAR(Err.c_str())), Err.empty());
	if (!Err.empty()) return false;
	TestTrue(TEXT("provinces"), Data.provinceCount > 10000);
	TestTrue(TEXT("countries"), Data.countries.size() >= 70);
	warroom::GameState S;
	TestTrue(TEXT("new game"), warroom::NewGame(Data, "test", "1936", S).empty());
	warroom::Advance(S, Data, 24 * 30);
	TestEqual(TEXT("30 days"), (int64)S.hour, (int64)(24 * 30));
	warroom::GameState Back;
	const std::string LoadErr = warroom::LoadFromString(warroom::SaveToString(S), Data, Back);
	TestTrue(TEXT("save round trip"), LoadErr.empty() && warroom::SaveToString(Back) == warroom::SaveToString(S));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarRoomDeterminismTest, "WarRoom.Sim.Determinism", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FWarRoomDeterminismTest::RunTest(const FString& Parameters)
{
	warroom::Data Data;
	if (!warroom::LoadData(TCHAR_TO_UTF8(*UWarRoomSubsystem::FindDataDir()), Data).empty()) return false;
	warroom::GameState A, B;
	warroom::NewGame(Data, "same", "1936", A); warroom::NewGame(Data, "same", "1936", B);
	warroom::Advance(A, Data, 24 * 60); for (int i = 0; i < 60; i++) warroom::Advance(B, Data, 24);
	TestTrue(TEXT("identical"), warroom::SaveToString(A) == warroom::SaveToString(B));
	return true;
}

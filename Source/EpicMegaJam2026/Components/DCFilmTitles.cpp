#include "Components/DCFilmTitles.h"

namespace
{
	struct FSkillWords
	{
		TArray<const TCHAR*> Adjectives;
		TArray<const TCHAR*> Nouns;
	};

	// Index = ESkill. Kept short: the whole title has to fit in about 20 characters.
	const FSkillWords& WordsFor(ESkill Skill)
	{
		static const FSkillWords Funny = {
			{TEXT("Goofy"), TEXT("Silly"), TEXT("Zany"), TEXT("Cheesy"), TEXT("Wacky"), TEXT("Tipsy"), TEXT("Clumsy"), TEXT("Sassy")},
			{TEXT("Clown"), TEXT("Mime"), TEXT("Pun"), TEXT("Llama"), TEXT("Banana"), TEXT("Intern"), TEXT("Karen"), TEXT("Meme")}};
		static const FSkillWords Scary = {
			{TEXT("Haunted"), TEXT("Cursed"), TEXT("Creepy"), TEXT("Evil"), TEXT("Spooky"), TEXT("Undead"), TEXT("Slimy"), TEXT("Rotten")},
			{TEXT("Ghost"), TEXT("Zombie"), TEXT("Basement"), TEXT("Landlord"), TEXT("Mannequin"), TEXT("Doll"), TEXT("Crypt"), TEXT("Dentist")}};
		static const FSkillWords Brave = {
			{TEXT("Fearless"), TEXT("Mighty"), TEXT("Heroic"), TEXT("Rowdy"), TEXT("Angry"), TEXT("Bold"), TEXT("Buff"), TEXT("Reckless")},
			{TEXT("Hero"), TEXT("Cop"), TEXT("Brawler"), TEXT("Sheriff"), TEXT("Gladiator"), TEXT("Ninja"), TEXT("Marine"), TEXT("Biker")}};
		static const FSkillWords Thoughtful = {
			{TEXT("Quiet"), TEXT("Moody"), TEXT("Deep"), TEXT("Pensive"), TEXT("Dreamy"), TEXT("Wistful"), TEXT("Brooding"), TEXT("Arty")},
			{TEXT("Poet"), TEXT("Monk"), TEXT("Professor"), TEXT("Void"), TEXT("Therapist"), TEXT("Hipster"), TEXT("Critic"), TEXT("Barista")}};
		static const FSkillWords Romantic = {
			{TEXT("Lovely"), TEXT("Steamy"), TEXT("Sappy"), TEXT("Smitten"), TEXT("Tender"), TEXT("Flirty"), TEXT("Swoony"), TEXT("Soppy")},
			{TEXT("Kiss"), TEXT("Wedding"), TEXT("Cupid"), TEXT("Date"), TEXT("Heart"), TEXT("Rose"), TEXT("Swan"), TEXT("Ex")}};
		switch (Skill)
		{
		case ESkill::Funny: return Funny;
		case ESkill::Scary: return Scary;
		case ESkill::Brave: return Brave;
		case ESkill::Thoughtful: return Thoughtful;
		default: return Romantic;
		}
	}

	// Sequel and reboot clichés that work after any noun
	const TArray<const TCHAR*> Tails = {TEXT("Returns"), TEXT("Reboot"), TEXT("Origins"), TEXT("Again"), TEXT("Forever"), TEXT("Unrated"),
		TEXT("Inc."), TEXT("Reloaded"), TEXT("Taxes"), TEXT("Brunch")};

	template <typename T> const TCHAR* Pick(const TArray<T>& Words) { return Words[FMath::RandHelper(Words.Num())]; }

	// Highest skill first, second only if it has points; a flat profile picks at random.
	void TopSkills(const FSkillVector& Profile, ESkill& OutFirst, ESkill& OutSecond)
	{
		TArray<ESkill> Order;
		for (int32 i = 0; i < DCNumSkills; ++i)
		{
			Order.Add(static_cast<ESkill>(i));
		}
		Order.Sort([&Profile](ESkill A, ESkill B) { return Profile.Get(A) > Profile.Get(B); });
		OutFirst = Profile.Get(Order[0]) > 0 ? Order[0] : static_cast<ESkill>(FMath::RandHelper(DCNumSkills));
		OutSecond = Profile.Get(Order[1]) > 0 ? Order[1] : OutFirst;
	}
}

FString DCFilmTitles::Generate(const FSkillVector& Profile, const TSet<FString>& Used, int32 MaxLength)
{
	ESkill First, Second;
	TopSkills(Profile, First, Second);
	const FSkillWords& A = WordsFor(First);
	const FSkillWords& B = WordsFor(Second);

	for (int32 Try = 0; Try < 32; ++Try)
	{
		const FString Adj = Pick(A.Adjectives);
		const FString Noun = Pick(FMath::RandBool() ? A.Nouns : B.Nouns);
		const int32 Num = FMath::RandRange(2, 5);
		FString Title;
		switch (FMath::RandHelper(5))
		{
		case 0: Title = FString::Printf(TEXT("%s %s"), *Adj, *Noun); break;
		case 1: Title = FString::Printf(TEXT("The %s %s"), *Adj, *Noun); break;
		case 2: Title = FString::Printf(TEXT("%s %s"), *Noun, Pick(Tails)); break;
		case 3: Title = FString::Printf(TEXT("%s %d: %s"), *Noun, Num, Pick(Tails)); break;
		default: Title = FString::Printf(TEXT("%s %s %d"), *Adj, *Noun, Num); break;
		}
		if (Title.Len() <= MaxLength && !Used.Contains(Title))
		{
			return Title;
		}
	}
	return FString();
}

#include "../DirectX11/CommandList.h"
#include "../DirectX11/IniHandler.h"
#include "../DirectX11/globals.h"

#include <clocale>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool result, const char *message)
{
	if (!result)
		throw std::runtime_error(message);
}

float evaluate(const wchar_t *text)
{
	CommandListExpression expression;
	const std::wstring input(text);
	float result = 0;
	require(expression.parse(&input, nullptr, nullptr), "valid expression was rejected");
	require(expression.static_evaluate(&result), "constant expression did not evaluate");
	return result;
}

void rejects_empty_groups()
{
	for (const wchar_t *text : {L"()", L"1 + (())"})
	{
		CommandListExpression expression;
		const std::wstring input(text);
		require(!expression.parse(&input, nullptr, nullptr), "empty group was accepted");
	}
	require(evaluate(L"(2 + 3) * 4") == 20.0f, "grouping changed arithmetic precedence");
}

void accepts_leading_dot_numbers()
{
	require(evaluate(L".125") == 0.125f, "leading-dot literal changed value");
	require(evaluate(L"-.125") == -0.125f, "negative leading-dot literal changed value");
	require(evaluate(L"2-.125") == 1.875f, "leading-dot literal broke subtraction");
	for (const wchar_t *text : {L".", L".1.2", L"1..2"})
	{
		CommandListExpression expression;
		const std::wstring input(text);
		require(!expression.parse(&input, nullptr, nullptr), "malformed numeric input was accepted");
	}
}

void preserves_pending_save()
{
	const std::filesystem::path root = std::filesystem::current_path() / L".quality" / L"settings-test";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	const auto blocker = root / L"not-a-directory";
	std::ofstream(blocker) << "block";
	CommandListVariable variable(L"$test", 7.5f, VariableFlags::PERSIST);
	persistent_variables.push_back(&variable);
	G->user_config = (blocker / L"settings.ini").wstring();
	G->user_config_dirty = 3;
	G->gTime = 12.0f;
	G->gDefaultLocale = "C";
	setlocale(LC_CTYPE, "C");
	SavePersistentSettings();
	const int dirty_after_failure = G->user_config_dirty;
	const std::string locale_after_failure = setlocale(LC_CTYPE, nullptr);
	G->user_config = (root / L"settings.ini").wstring();
	SavePersistentSettings();
	std::ifstream saved(root / L"settings.ini");
	const std::string contents((std::istreambuf_iterator<char>(saved)), std::istreambuf_iterator<char>());
	saved.close();
	persistent_variables.clear();
	std::filesystem::remove_all(root);
	require(dirty_after_failure == 3, "failed save discarded pending settings");
	require(locale_after_failure == "C", "failed save changed the caller's locale");
	require(contents.find("$test = 7.5\n") != std::string::npos, "retry did not save the pending value");
	require(G->user_config_dirty == 0, "successful save left settings dirty");
	require(G->gSettingsSaveTime == 12.0f, "save scheduling changed");
}
}

int main(int argc, char **argv)
{
	G->gShowWarnings = false;
	const struct
	{
		const char *name;
		void (*run)();
	} tests[] = {{"empty-groups", rejects_empty_groups},
	             {"leading-dot", accepts_leading_dot_numbers},
	             {"pending-save", preserves_pending_save}};
	int failures = 0;
	for (const auto &test : tests)
	{
		if (argc > 1 && std::string(argv[1]) != test.name)
			continue;
		try
		{
			test.run();
			std::cout << "PASS " << test.name << '\n';
		}
		catch (const std::exception &error)
		{
			std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
			++failures;
		}
	}
	return failures ? 1 : 0;
}

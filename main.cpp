#include <iostream>

// TIP コードを<b>Run</b>するには、<shortcut actionId="Run"/> を押すか、ガターにある <icon src="AllIcons.Actions.Execute"/> アイコンをクリックします。
int main() {
    // TIP キャレットが <b>lang</b> 変数名にある状態で <shortcut actionId="RenameElement"/> を押すと、CLion によって変数名が変更されます。
    auto lang = "C++";
    std::cout << "Hello and welcome to " << lang << "!\n";

    for (int i = 1; i <= 5; i++) {
        // TIP <shortcut actionId="Debug"/> を押すと、コードのデバッグが開始されます。1 つの <icon src="AllIcons.Debugger.Db_set_breakpoint"/> ブレークポイントが設定されていますが、<shortcut actionId="ToggleLineBreakpoint"/> を押していつでも追加できます。
        std::cout << "i = " << i << std::endl;
    }

    return 0;
    // TIP <a href="https://www.jetbrains.com/help/clion/">jetbrains.com/help/clion/</a> で CLion ヘルプをご覧ください。メインメニューから 'ヘルプ | IDE 機能の学習' を選択して CLion の対話型レッスンを試すこともできます。
}
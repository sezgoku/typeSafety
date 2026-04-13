#include <clang-c/Index.h>
#include <iostream>

int main(){
  CXIndex index = clang_createIndex(0, 0);
  CXTranslationUnit unit = clang_parseTranslationUnit(
    	index,
    	"foo.cpp", nullptr, 0,
    	nullptr, 0,
    	CXTranslationUnit_None);

  if (unit == nullptr){
    	std::cout << "cannot parse\n";
    	return 0;
  }
  CXCursor cursor = clang_getTranslationUnitCursor(unit);

  clang_visitChildren(
  	cursor,
  	[](CXCursor current_cursor, CXCursor parent, CXClientData client_data){
    	CXType cursor_type = clang_getCursorType(current_cursor);

    	CXString type_kind_spelling = clang_getTypeKindSpelling(cursor_type.kind);
    	std::cout << "Type Kind: " << clang_getCString(type_kind_spelling);
    	clang_disposeString(type_kind_spelling);

    	if(cursor_type.kind == CXType_Pointer ||                     
	  	cursor_type.kind == CXType_LValueReference ||              
		cursor_type.kind == CXType_RValueReference){               
      		CXType pointed_to_type = clang_getPointeeType(cursor_type);

      		CXString pointed_to_type_spelling = clang_getTypeSpelling(pointed_to_type);     
      		std::cout << "pointing to type: " << clang_getCString(pointed_to_type_spelling);
      		clang_disposeString(pointed_to_type_spelling);
    }
    	else if(cursor_type.kind == CXType_Record){
      		CXString type_spelling = clang_getTypeSpelling(cursor_type);
      		std::cout <<  ", namely " << clang_getCString(type_spelling);
      		clang_disposeString(type_spelling);
    }
    std::cout << "\n";
    return CXChildVisit_Recurse;
  },
  nullptr
  );
}

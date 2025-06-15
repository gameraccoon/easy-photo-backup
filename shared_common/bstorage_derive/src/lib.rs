mod private;

use proc_macro::TokenStream;

#[proc_macro_derive(ToValueByOrder, attributes(bstorage))]
pub fn to_value_by_order_derive(input: TokenStream) -> TokenStream {
    let ast = syn::parse(input).unwrap();

    private::impl_to_value_by_order(&ast)
}

#[proc_macro_derive(FromValueByOrder, attributes(bstorage))]
pub fn from_value_by_order_derive(input: TokenStream) -> TokenStream {
    let ast = syn::parse(input).unwrap();

    private::impl_from_value_by_order(&ast)
}

#[proc_macro_derive(ToValueByName, attributes(bstorage))]
pub fn to_value_by_name_derive(input: TokenStream) -> TokenStream {
    let ast = syn::parse(input).unwrap();

    private::impl_to_value_by_name(&ast)
}

#[proc_macro_derive(FromValueByName, attributes(bstorage))]
pub fn from_value_by_name_derive(input: TokenStream) -> TokenStream {
    let ast = syn::parse(input).unwrap();

    private::impl_from_value_by_name(&ast)
}

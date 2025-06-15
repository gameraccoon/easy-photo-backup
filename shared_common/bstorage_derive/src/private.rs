use proc_macro::TokenStream;
use quote::quote;

fn is_byte_array(field: &syn::Field) -> bool {
    field.attrs.iter().any(|attr| {
        if attr.path().is_ident("bstorage") {
            match attr.parse_args::<syn::Path>() {
                Ok(path) => path.is_ident("byte_array"),
                Err(_) => false,
            }
        } else {
            false
        }
    })
}

pub(crate) fn impl_to_value_by_order(ast: &syn::DeriveInput) -> TokenStream {
    let name = &ast.ident;

    let generated = match &ast.data {
        syn::Data::Struct(data_struct) => match &data_struct.fields {
            syn::Fields::Named(fields_named) => {
                let fields = fields_named.named.iter().map(|field| {
                    let field_name = &field.ident;
                    if is_byte_array(field) {
                        quote! {
                            bstorage::Value::ByteArray(self.#field_name.clone()),
                        }
                    } else {
                        quote! {
                            self.#field_name.to_value(),
                        }
                    }
                });
                quote! {
                    impl ToValue for #name {
                        fn to_value(&self) -> bstorage::Value {
                            bstorage::Value::Tuple(Vec::from([
                                #(#fields)*
                            ]))
                        }
                    }
                }
            }
            syn::Fields::Unnamed(fields_unnamed) => {
                let fields = (0..fields_unnamed.unnamed.len()).map(|field_index| {
                    let field_index = syn::Index::from(field_index);
                    quote! {
                        self.#field_index.to_value(),
                    }
                });
                quote! {
                    impl ToValue for #name {
                        fn to_value(&self) -> bstorage::Value {
                            bstorage::Value::Tuple(Vec::from([
                                #(#fields)*
                            ]))
                        }
                    }
                }
            }
            syn::Fields::Unit => {
                return quote! {
                    compile_error!("Unit structs or fields are not supported");
                }
                .into();
            }
        },
        syn::Data::Enum(_data_enum) => {
            return quote! {
                compile_error!("Enums are not supported for now");
            }
            .into();
        }
        syn::Data::Union(_data_union) => {
            return quote! {
                compile_error!("Unions are not supported");
            }
            .into();
        }
    };

    generated.into()
}

pub(crate) fn impl_from_value_by_order(ast: &syn::DeriveInput) -> TokenStream {
    let name = &ast.ident;
    let name_str = name.to_string();

    let generated = match &ast.data {
        syn::Data::Struct(data_struct) => match &data_struct.fields {
            syn::Fields::Named(fields_named) => {
                let fields = fields_named.named.iter().map(|field| {
                    let field_name = &field.ident;
                    let field_name_str = field_name.clone().map(|field_name| field_name.to_string());
                    let field_type = &field.ty;
                    if is_byte_array(field) {
                        quote! {
                            #field_name: match iter.next() {
                                Some(value) => {
                                    match value {
                                        bstorage::Value::ByteArray(value) => value,
                                        _ => {
                                            return Err(format!("Field {} of struct {} expected to be a byte array, but it is not", #field_name_str, #name_str));
                                        }
                                    }
                                },
                                None => {
                                    return Err(format!("{} is missing field with name '{}'", #name_str, #field_name_str));
                                }
                            },
                        }
                    } else {
                        quote! {
                            #field_name: match iter.next() {
                                Some(value) => {
                                    match value.to_rust_type::<#field_type>() {
                                        Ok(value) => value,
                                        Err(e) => {
                                            return Err(format!("{} /=>/ Failed to deserialize field {} of struct {}", e, #field_name_str, #name_str));
                                        }
                                    }
                                },
                                None => {
                                    return Err(format!("{} is missing field with name '{}'", #name_str, #field_name_str));
                                }
                            },
                        }
                    }
                });
                quote! {
                    impl FromValue for #name {
                        fn from_value(value: bstorage::Value) -> Result<Self, String> {
                            match value {
                                bstorage::Value::Tuple(values) => {
                                    let mut iter = values.into_iter();
                                    Ok(#name {
                                        #(#fields)*
                                    })
                                }
                                _ => {
                                    Err(format!("Expected {} to be a tuple", #name_str))
                                }
                            }
                        }
                    }
                }
            }
            syn::Fields::Unnamed(fields_unnamed) => {
                let fields = fields_unnamed.unnamed.iter().enumerate().map(|(field_index, field)| {
                        let field_type = &field.ty;
                        quote! {
                            match iter.next() {
                                Some(value) => {
                                    match #field_type::from_value(value) {
                                        Ok(value) => value,
                                        Err(e) => {
                                            return Err(format!("{} /=>/ Failed to deserialize positional value at index {} of struct {}", e, #field_index, #name_str));
                                        }
                                    }
                                }
                                None => {
                                    return Err(format!("{} is missing positional value at index {}", #name_str, #field_index));
                                }
                            },
                        }
                    });
                quote! {
                    impl FromValue for #name {
                        fn from_value(value: bstorage::Value) -> Result<Self, String> {
                            match value {
                                bstorage::Value::Tuple(values) => {
                                    let mut iter = values.into_iter();
                                    Ok(#name(
                                        #(#fields)*
                                    ))
                                }
                                _ => {
                                    Err(format!("Expected {} to be a tuple", #name_str))
                                }
                            }
                        }
                    }
                }
            }
            syn::Fields::Unit => {
                return quote! {
                    compile_error!("Unit structs or fields are not supported");
                }
                .into();
            }
        },
        syn::Data::Enum(_data_enum) => {
            return quote! {
                compile_error!("Enums are not supported for now");
            }
            .into();
        }
        syn::Data::Union(_data_union) => {
            return quote! {
                compile_error!("Unions are not supported");
            }
            .into();
        }
    };

    generated.into()
}

pub(crate) fn impl_to_value_by_name(ast: &syn::DeriveInput) -> TokenStream {
    let name = &ast.ident;

    let generated = match &ast.data {
        syn::Data::Struct(data_struct) => {
            match &data_struct.fields {
                syn::Fields::Named(fields_named) => {
                    let fields = fields_named.named.iter().map(|field| {
                        let field_name = &field.ident;
                        let field_name_str = field_name.clone().map(|field_name| field_name.to_string());
                        if is_byte_array(field) {
                            quote! {
                                (#field_name_str.to_string(), bstorage::Value::ByteVec(self.#field_name.clone())),
                            }
                        } else {
                            quote! {
                                (#field_name_str.to_string(), self.#field_name.to_value()),
                            }
                        }
                    });
                    quote! {
                        impl ToValue for #name {
                            fn to_value(&self) -> bstorage::Value {
                                bstorage::Value::Object(std::collections::HashMap::from([
                                    #(#fields)*
                                ]))
                            }
                        }
                    }
                }
                syn::Fields::Unnamed(_fields_unnamed) => {
                    return quote! {
                        compile_error!("Tuple structs or tuple values are not supported for conversions by field name. Use conversions by order instead.");
                    }.into()
                }
                syn::Fields::Unit => {
                    return quote! {
                        compile_error!("Unit structs or fields are not supported");
                    }.into()
                }
            }
        }
        syn::Data::Enum(_data_enum) => {
            return quote! {
                compile_error!("Enums are not supported for now");
            }.into()
        }
        syn::Data::Union(_data_union) => {
            return quote! {
                compile_error!("Unions are not supported");
            }.into()
        }
    };

    generated.into()
}

pub(crate) fn impl_from_value_by_name(ast: &syn::DeriveInput) -> TokenStream {
    let name = &ast.ident;
    let name_str = name.to_string();

    let generated = match &ast.data {
        syn::Data::Struct(data_struct) => match &data_struct.fields {
            syn::Fields::Named(fields_named) => {
                let fields = fields_named.named.iter().map(|field| {
                    let field_name = &field.ident;
                    let field_name_str = field_name.clone().map(|field_name| field_name.to_string());
                    let field_type = &field.ty;
                    if is_byte_array(field) {
                        quote! {
                            #field_name: match values.remove(&#field_name_str.to_string()) {
                                Some(value) => match value {
                                    bstorage::Value::ByteArray(value) => value,
                                    _ => {
                                        return Err(format!("Field {} of struct {} expected to be a byte array, but it is not", #field_name_str, #name_str));
                                    }
                                },
                                None => {
                                    return Err(format!("{} is missing field with name '{}'", #name_str, #field_name_str));
                                }
                            },
                        }
                    } else {
                        quote! {
                            #field_name: match values.remove(&#field_name_str.to_string()) {
                                Some(value) => match value.to_rust_type::<#field_type>() {
                                    Ok(value) => value,
                                    Err(e) => {
                                        return Err(format!("{} /=>/ Failed to deserialize field {} of struct {}", e, #field_name_str, #name_str));
                                    }
                                }
                                None => {
                                    return Err(format!("{} is missing field with name '{}'", #name_str, #field_name_str));
                                }
                            },
                        }
                    }
                });
                quote! {
                    impl FromValue for #name {
                        fn from_value(value: bstorage::Value) -> Result<Self, String> {
                            match value {
                                bstorage::Value::Object(values) => {
                                    let mut values = values;
                                    let result = Ok(#name {
                                        #(#fields)*
                                    });

                                    if !values.is_empty() {
                                        return Err(format!("Unexpected fields in {}: {}", #name_str, values.keys().map(|key| key.to_string()).collect::<Vec<_>>().join(", ")));
                                    }

                                    result
                                }
                                _ => {
                                    Err(format!("Expected {} to be an object", #name_str))
                                }
                            }
                        }
                    }
                }
            }
            syn::Fields::Unnamed(_fields_unnamed) => {
                return quote! {
                    compile_error!("Tuple structs or tuple values are not supported for conversions by field name. Use conversions by order instead.");
                }.into()
            }
            syn::Fields::Unit => {
                return quote! {
                    compile_error!("Unit structs or fields are not supported");
                }
                .into();
            }
        },
        syn::Data::Enum(_data_enum) => {
            return quote! {
                compile_error!("Enums are not supported for now");
            }
            .into();
        }
        syn::Data::Union(_data_union) => {
            return quote! {
                compile_error!("Unions are not supported");
            }
            .into();
        }
    };

    generated.into()
}

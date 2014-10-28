// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
*/

#pragma once

class FSlateOpenGLTexture : public TSlateTexture< GLuint >, public FSlateUpdatableTexture
{
public:
	FSlateOpenGLTexture( uint32 InSizeX, uint32 InSizeY )
		: TSlateTexture( FSlateOpenGLTexture::NullTexture )
		, SizeX(InSizeX)
		, SizeY(InSizeY)
	{

	}

	void Init( GLenum Format, const TArray<uint8>& TextureData );

	void Init( GLuint TextureID );

	~FSlateOpenGLTexture()
	{
		glDeleteTextures( 1, &ShaderResource );
	}

	uint32 GetWidth() const { return SizeX; }
	uint32 GetHeight() const { return SizeY; }

	// FSlateUpdatableTexture interface
	virtual FSlateShaderResource* GetSlateResource() override {return this;}
	virtual void ResizeTexture( uint32 Width, uint32 Height ) override;
	virtual void UpdateTexture(const TArray<uint8>& Bytes) override;

private:
	static GLuint NullTexture;

	uint32 SizeX;
	uint32 SizeY;
};

/** 
 * Representation of a texture for fonts in which characters are packed tightly based on their bounding rectangle 
 */
class FSlateFontTextureOpenGL : public FSlateFontAtlas
{
public:
	FSlateFontTextureOpenGL( uint32 Width, uint32 Height );
	~FSlateFontTextureOpenGL();

	void CreateFontTexture();

	/** FSlateFontAtlas interface */
	virtual void ConditionalUpdateTexture();
	virtual class FSlateShaderResource* GetSlateTexture() override { return FontTexture; }
	virtual class FTextureResource* GetEngineTexture() override { return nullptr; }
private:
	FSlateOpenGLTexture* FontTexture;
};

